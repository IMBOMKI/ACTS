// This file is part of the Acts project.
//
// Copyright (C) 2018 CERN for the benefit of the Acts project
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "Acts/Seeding/InternalSeed.hpp"
#include "Acts/Seeding/InternalSpacePoint.hpp"
#include "Acts/Seeding/SeedfinderConfig.hpp"

#include <array>
#include <list>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include "Acts/Seeding/SeedFilter.hpp"

// --- CUDA headers --- //
#include "Acts/Utilities/Platforms/PlatformDef.h"
#include "cuda_runtime_api.h"

namespace Acts {
struct LinCircle {
  float Zo;
  float cotTheta;
  float iDeltaR;
  float Er;
  float U;
  float V;
};
  template <typename external_spacepoint_t, typename platform_t>
class Seedfinder {
  ///////////////////////////////////////////////////////////////////
  // Public methods:
  ///////////////////////////////////////////////////////////////////

 using Platform_t = platform_t;
    
 public:
  /// The only constructor. Requires a config object.
  /// @param config the configuration for the Seedfinder

  Seedfinder(Acts::SeedfinderConfig<external_spacepoint_t> config);

  ~Seedfinder() = default;
  /**    @name Disallow default instantiation, copy, assignment */
  //@{
  Seedfinder() = delete;
  Seedfinder(const Seedfinder<external_spacepoint_t, platform_t>&) = delete;
  Seedfinder<external_spacepoint_t, platform_t >& operator=(
  const Seedfinder<external_spacepoint_t, platform_t>&) = delete;
  //@}

  /// Create all seeds from the space points in the three iterators.
  /// Can be used to parallelize the seed creation
  /// @param bottom group of space points to be used as innermost SP in a seed.
  /// @param middle group of space points to be used as middle SP in a seed.
  /// @param top group of space points to be used as outermost SP in a seed.
  /// Ranges must return pointers.
  /// Ranges must be separate objects for each parallel call.
  /// @return vector in which all found seeds for this group are stored.
//template< typename sp_range_t >
  template< typename T=Platform_t, typename sp_range_t>
  typename std::enable_if< std::is_same<T, Acts::CPU>::value, std::vector<Seed<external_spacepoint_t> > >::type
  createSeedsForGroup(sp_range_t bottomSPs, sp_range_t middleSPs, sp_range_t topSPs) const;

  template< typename T=Platform_t, typename sp_range_t>
  typename std::enable_if< std::is_same<T, Acts::CUDA>::value, std::vector<Seed<external_spacepoint_t> > >::type
  createSeedsForGroup(sp_range_t bottomSPs, sp_range_t middleSPs, sp_range_t topSPs) const;
    
 private:

  Acts::SeedfinderConfig<external_spacepoint_t> m_config;
};

}  // namespace Acts

#include "Acts/Seeding/Seedfinder.ipp"
