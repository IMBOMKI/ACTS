// This file is part of the Acts project.
//
// Copyright (C) 2018 CERN for the benefit of the Acts project
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <memory>
#include <mutex>
#include <queue>
#include <vector>

#include "Acts/Seeding/IExperimentCuts.hpp"
#include "Acts/Seeding/InternalSeed.hpp"
#include "Acts/Seeding/Seed.hpp"

namespace Acts {
struct SeedFilterConfig {
  // the allowed delta between two inverted seed radii for them to be considered
  // compatible.
  float deltaInvHelixDiameter = 0.00003;
  // the impact parameters (d0) is multiplied by this factor and subtracted from
  // weight
  float impactWeightFactor = 1.;
  // seed weight increased by this value if a compatible seed has been found.
  float compatSeedWeight = 200.;
  // minimum distance between compatible seeds to be considered for weight boost
  float deltaRMin = 5.;
  // in dense environments many seeds may be found per middle space point.
  // only seeds with the highest weight will be kept if this limit is reached.
  unsigned int maxSeedsPerSpM = 10;
  // how often do you want to increase the weight of a seed for finding a
  // compatible seed?
  size_t compatSeedLimit = 2;
  // Tool to apply experiment specific cuts on collected middle space points
};

/// @class Filter seeds at various stages with the currently
/// available information.
template <typename external_spacepoint_t>
class SeedFilter {
 public:
  SeedFilter(SeedFilterConfig config,
             IExperimentCuts<external_spacepoint_t>* expCuts = 0);

  SeedFilter() = delete;
  virtual ~SeedFilter() = default;

  /// Create InternalSeeds for the all seeds with the same bottom and middle
  /// space point and discard all others.
  /// @param bottomSP fixed bottom space point
  /// @param middleSP fixed middle space point
  /// @param topSpVec vector containing all space points that may be compatible
  /// with both bottom and middle space point
  /// @param origin on the z axis as defined by bottom and middle space point
  /// @return vector of pairs containing seed weight and seed for all valid
  /// created seeds
  virtual std::vector<std::pair<
      float, std::unique_ptr<const InternalSeed<external_spacepoint_t>>>>
  filterSeeds_2SpFixed(
      const InternalSpacePoint<external_spacepoint_t>& bottomSP,
      const InternalSpacePoint<external_spacepoint_t>& middleSP,
      std::vector<const InternalSpacePoint<external_spacepoint_t>*>& topSpVec,
      std::vector<float>& invHelixDiameterVec,
      std::vector<float>& impactParametersVec, float zOrigin) const;

  /// Filter seeds once all seeds for one middle space point have been created
  /// @param seedsPerSpM vector of pairs containing weight and seed for all
  /// for all seeds with the same middle space point
  /// @return vector of all InternalSeeds that not filtered out
  virtual void filterSeeds_1SpFixed(
	  std::vector<std::pair<
          float, std::unique_ptr<const InternalSeed<external_spacepoint_t>>>>&
          seedsPerSpM,
      std::vector<Seed<external_spacepoint_t>>& outVec) const;

  SeedFilterConfig GetConfiguration(){return m_cfg;}
  
 private:
  const SeedFilterConfig m_cfg;
  const IExperimentCuts<external_spacepoint_t>* m_experimentCuts;
};

#ifdef __CUDACC__
#define CUDA_HOSTDEV __host__ __device__
#else
#define CUDA_HOSTDEV
#endif
  
class CuSeedFilter{
public:
  CuSeedFilter()=default;
  CuSeedFilter(SeedFilterConfig config,
	       CuIExperimentCuts expCuts);

  
  CUDA_HOSTDEV void filterSeeds_2SpFixed(const int*   threadId,
					 const float* spM,
					 const float* spB,
					 const int*   nSpT,
					 const float* spTmat,
					 const bool*  isPassed,
					 const float* invHelixDiameterVec,
					 const float* impactParametersVec,
					 const float* Zob,
					 float* weight,
					 bool* isTriplet
					 ){
    isTriplet[*threadId] = false;
    
    float invHelixDiameter = invHelixDiameterVec[*threadId];
    float lowerLimitCurv = invHelixDiameter - m_cfg.deltaInvHelixDiameter;
    float upperLimitCurv = invHelixDiameter + m_cfg.deltaInvHelixDiameter;
    float currentTop_r = spTmat[*threadId+(*nSpT)*3];
    float impact = impactParametersVec[*threadId];
    
    // declare array of seed compatibility  
    
    bool* isCompatibleSeed = new bool[*nSpT];
    
    for (size_t j = 0; j < *nSpT; j++){
      isCompatibleSeed[j] = false;
    }
    
    weight[*threadId] = -(impact * m_cfg.impactWeightFactor);
    for (size_t j = 0; j < *nSpT; j++) {

      if (isPassed[j] == false){
	continue;
      }
      
      if (j == *threadId) {
	continue;
      }
      
      // compared top SP should have at least deltaRMin distance
      float otherTop_r = spTmat[j+(*nSpT)*3];
      float deltaR = currentTop_r - otherTop_r;
      if (fabs(deltaR) < m_cfg.deltaRMin) {
        continue;
      }
      // curvature difference within limits?
      // TODO: how much slower than sorting all vectors by curvature
      // and breaking out of loop? i.e. is vector size large (e.g. in jets?)
      if (invHelixDiameterVec[j] < lowerLimitCurv) {
        continue;
      }
      if (invHelixDiameterVec[j] > upperLimitCurv) {
        continue;
      }
      
      bool newCompSeed = true;

      for (int k=0; k<*nSpT; k++){
	if (isCompatibleSeed[k] == true){
	  float previousDiameter = spTmat[k+(*nSpT)*3];	  
	  if (fabs(previousDiameter - otherTop_r) < m_cfg.deltaRMin) {
	    newCompSeed = false;
	    break;
	  }
	}
      }

      if (newCompSeed) {
	isCompatibleSeed[j] = true;
        weight[*threadId] += m_cfg.compatSeedWeight;
      }

      int NumOfCompatibleSeeds = 0;
      for (int k=0; k<*nSpT; k++){
	if (isCompatibleSeed[k] == true){
	  NumOfCompatibleSeeds++;
	}
      }
      
      if (NumOfCompatibleSeeds >= m_cfg.compatSeedLimit) {
        break;
      }      
    }

    float spT[6];
    spT[0] = spTmat[*threadId+(*nSpT)*0];
    spT[1] = spTmat[*threadId+(*nSpT)*1];
    spT[2] = spTmat[*threadId+(*nSpT)*2];
    spT[3] = spTmat[*threadId+(*nSpT)*3];
    spT[4] = spTmat[*threadId+(*nSpT)*4];
    spT[5] = spTmat[*threadId+(*nSpT)*5];
          
    weight[*threadId] += m_experimentCuts.seedWeight(spB, spM, spT);
    // discard seeds according to detector specific cuts (e.g.: weight)
    if (!m_experimentCuts.singleSeedCut(weight[*threadId], spB, spM, spT)) {
      isTriplet[*threadId] = true;
    }
    
  }
  
  SeedFilterConfig  m_cfg;
  CuIExperimentCuts m_experimentCuts;
};
  
}  // namespace Acts
#include "Acts/Seeding/SeedFilter.ipp"
