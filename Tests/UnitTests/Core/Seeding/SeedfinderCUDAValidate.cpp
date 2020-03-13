// This file is part of the Acts project.
//
// Copyright (C) 2019 CERN for the benefit of the Acts project
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <chrono>
#include <fstream>
#include <iostream>
#include <sstream>
#include <utility>

#include <boost/type_erasure/any_cast.hpp>

#include "Acts/Seeding/BinFinder.hpp"
#include "Acts/Seeding/BinnedSPGroup.hpp"
#include "Acts/Seeding/InternalSeed.hpp"
#include "Acts/Seeding/InternalSpacePoint.hpp"
#include "Acts/Seeding/Seed.hpp"
#include "Acts/Seeding/SeedFilter.hpp"
#include "Acts/Seeding/Seedfinder.hpp"
#include "Acts/Seeding/SpacePointGrid.hpp"

#include "ATLASCuts.hpp"
#include "SpacePoint.hpp"

// --- CUDA headers --- //
#include "cuda.h"
#include "Acts/Utilities/Platforms/CUDA/CuUtils.cu"

std::vector<const SpacePoint*> readFile(std::string filename) {
  std::string line;
  int layer;
  std::vector<const SpacePoint*> readSP;

  std::ifstream spFile(filename);
  if (spFile.is_open()) {
    while (!spFile.eof()) {
      std::getline(spFile, line);
      std::stringstream ss(line);
      std::string linetype;
      ss >> linetype;
      int index;
      float x, y, z, r, varianceR, varianceZ;
      if (linetype == "lxyz") {
        ss >> index >> layer >> x >> y >> z >> varianceR >> varianceZ;
	//ss >> layer >> x >> y >> z >> varianceR >> varianceZ;
        r = std::sqrt(x * x + y * y);
        float f22 = varianceR;
        float wid = varianceZ;
        float cov = wid * wid * .08333;
        if (cov < f22)
          cov = f22;
        if (std::abs(z) > 450.) {
          varianceZ = 9. * cov;
          varianceR = .06;
        } else {
          varianceR = 9. * cov;
          varianceZ = .06;
        }
        SpacePoint* sp =
            new SpacePoint{x, y, z, r, layer, varianceR, varianceZ};
        //     if(r < 200.){
        //       sp->setClusterList(1,0);
        //     }
        readSP.push_back(sp);
      }
    }
  }
  return readSP;
}

int main() {
  
  std::string devName;
  SetDevice(0, devName);

  //std::vector<const SpacePoint*> spVec = readFile("hits4seeding_21100.csv");
  std::vector<const SpacePoint*> spVec = readFile("sample_1000k.txt");
  //std::vector<const SpacePoint*> spVec = readFile("sp.txt");
  std::cout << "size of read SP: " << spVec.size() << std::endl;

  Acts::SeedfinderConfig<SpacePoint> config;
  config.simpleTest = true;
  int nGroupsToIterate       = 70;
  config.nMiddleSPsToIterate = 1;

  // silicon detector max
  config.rMax = 160.;
  config.deltaRMin = 5.;
  config.deltaRMax = 160.;
  config.collisionRegionMin = -250.;
  config.collisionRegionMax = 250.;
  config.zMin = -2800.;
  config.zMax = 2800.;
  config.maxSeedsPerSpM = 5;
  // 2.7 eta
  config.cotThetaMax = 7.40627;
  config.sigmaScattering = 1.00000;

  config.minPt = 500.;
  config.bFieldInZ = 0.00199724;

  config.beamPos = {-.5, -.5};
  config.impactMax = 10.;

  auto bottomBinFinder = std::make_shared<Acts::BinFinder<SpacePoint>>(
      Acts::BinFinder<SpacePoint>());
  auto topBinFinder = std::make_shared<Acts::BinFinder<SpacePoint>>(
      Acts::BinFinder<SpacePoint>());
  Acts::SeedFilterConfig sfconf;
  Acts::ATLASCuts<SpacePoint> atlasCuts = Acts::ATLASCuts<SpacePoint>();
  config.seedFilter = std::make_unique<Acts::SeedFilter<SpacePoint>>(
      Acts::SeedFilter<SpacePoint>(sfconf, &atlasCuts));

  Acts::Seedfinder<SpacePoint, Acts::CPU>  a_cpu(config);
  Acts::Seedfinder<SpacePoint, Acts::CUDA> a_cuda(config);

  // covariance tool, sets covariances per spacepoint as required
  auto ct = [=](const SpacePoint& sp, float, float, float) -> Acts::Vector2D {
    return {sp.varianceR, sp.varianceZ};
  };

  // setup spacepoint grid config
  Acts::SpacePointGridConfig gridConf;
  gridConf.bFieldInZ = config.bFieldInZ;
  gridConf.minPt = config.minPt;
  gridConf.rMax = config.rMax;
  gridConf.zMax = config.zMax;
  gridConf.zMin = config.zMin;
  gridConf.deltaRMax = config.deltaRMax;
  gridConf.cotThetaMax = config.cotThetaMax;
  // create grid with bin sizes according to the configured geometry
  std::unique_ptr<Acts::SpacePointGrid<SpacePoint>> grid =
      Acts::SpacePointGridCreator::createGrid<SpacePoint>(gridConf);
  auto spGroup = Acts::BinnedSPGroup<SpacePoint>(spVec.begin(), spVec.end(), ct,
                                                 bottomBinFinder, topBinFinder,
                                                 std::move(grid), config);

  // --------- CUDA -------- //
  auto start_cuda       = std::chrono::system_clock::now();  
  std::vector<std::vector<Acts::Seed<SpacePoint>>> seedVector_cuda;
  int i_group_cuda = 0;
  auto groupIt_cuda     = spGroup.begin();
  auto endOfGroups_cuda = spGroup.end();
  for (; !(groupIt_cuda == endOfGroups_cuda); ++groupIt_cuda) {

    seedVector_cuda.push_back(a_cuda.createSeedsForGroup(groupIt_cuda.bottom(), 
							 groupIt_cuda.middle(), 
							 groupIt_cuda.top()));
    i_group_cuda++;
    if (i_group_cuda == nGroupsToIterate) break;
  }
  
  auto end_cuda = std::chrono::system_clock::now();  
  
  // --------- CPU -------- //
  auto start_cpu       = std::chrono::system_clock::now();
  std::vector<std::vector<Acts::Seed<SpacePoint>>> seedVector_cpu;
  
  int i_group_cpu = 0;
  auto groupIt_cpu     = spGroup.begin();
  auto endOfGroups_cpu = spGroup.end();
  for (; !(groupIt_cpu == endOfGroups_cpu); ++groupIt_cpu) {
    seedVector_cpu.push_back(a_cpu.createSeedsForGroup(groupIt_cpu.bottom(),
						       groupIt_cpu.middle(), 
						       groupIt_cpu.top()));   

    i_group_cpu++;
    if (i_group_cpu == nGroupsToIterate) break;
  }  
  
  auto end_cpu = std::chrono::system_clock::now();
  

  std::chrono::duration<double> elapsed_seconds_cpu = end_cpu - start_cpu;
  std::chrono::duration<double> elapsed_seconds_cuda = end_cuda - start_cuda;
  std::cout << "CPU  time to create seeds: " << elapsed_seconds_cpu.count() << std::endl;
  std::cout << "CUDA time to create seeds: " << elapsed_seconds_cuda.count() << std::endl;
  std::cout << "Number of regions: " << seedVector_cpu.size() << std::endl;
  int numSeeds = 0;
  for (auto& outVec : seedVector_cpu) {
    numSeeds += outVec.size();
  }
  
  std::cout << "Number of seeds generated: " << numSeeds << std::endl;
  for (auto& regionVec : seedVector_cpu) {
    for (size_t i = 0; i < regionVec.size(); i++) {
      const Acts::Seed<SpacePoint>* seed = &regionVec[i];
      const SpacePoint* sp = seed->sp()[0];
      std::cout << " (" << sp->x() << ", " << sp->y() << ", " << sp->z()
                << ") ";
      sp = seed->sp()[1];
      std::cout << sp->surface << " (" << sp->x() << ", " << sp->y() << ", "
                << sp->z() << ") ";
      sp = seed->sp()[2];
      std::cout << sp->surface << " (" << sp->x() << ", " << sp->y() << ", "
                << sp->z() << ") ";
      std::cout << std::endl;
    }
  }
  
  return 0;
}
