/**
 * Copyright Soramitsu Co., Ltd. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef CPP_FILECOIN_CORE_VM_ACTOR_BUILTIN_MINER_POLICY_HPP
#define CPP_FILECOIN_CORE_VM_ACTOR_BUILTIN_MINER_POLICY_HPP

#include "primitives/chain_epoch/chain_epoch.hpp"

namespace fc::vm::actor::builtin::miner {
  using primitives::ChainEpoch;

  constexpr ChainEpoch kPoStLookback{1};

  constexpr auto kElectionLookback{kPoStLookback};

  constexpr auto kWorkerKeyChangeDelay{2 * kElectionLookback};
}  // namespace fc::vm::actor::builtin::miner

#endif  // CPP_FILECOIN_CORE_VM_ACTOR_BUILTIN_MINER_POLICY_HPP
