/**
 * Copyright Soramitsu Co., Ltd. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef CPP_FILECOIN_CORE_VM_ACTOR_BUILTIN_MARKET_ACTOR_HPP
#define CPP_FILECOIN_CORE_VM_ACTOR_BUILTIN_MARKET_ACTOR_HPP

#include "codec/cbor/streams_annotation.hpp"
#include "primitives/chain_epoch/chain_epoch.hpp"
#include "primitives/types.hpp"
#include "vm/actor/actor_method.hpp"

namespace fc::vm::actor::builtin::market {
  using primitives::ChainEpoch;
  using primitives::DealId;

  constexpr MethodNumber kVerifyDealsOnSectorProveCommitMethodNumber{6};

  struct VerifyDealsOnSectorProveCommitParams {
    std::vector<DealId> deals;
    ChainEpoch sector_expiry;
  };

  CBOR_TUPLE(VerifyDealsOnSectorProveCommitParams, deals, sector_expiry)
}  // namespace fc::vm::actor::builtin::market

#endif  // CPP_FILECOIN_CORE_VM_ACTOR_BUILTIN_MARKET_ACTOR_HPP
