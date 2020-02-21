/**
 * Copyright Soramitsu Co., Ltd. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef CPP_FILECOIN_CORE_VM_ACTOR_BUILTIN_MINER_MINER_ACTOR_HPP
#define CPP_FILECOIN_CORE_VM_ACTOR_BUILTIN_MINER_MINER_ACTOR_HPP

#include "codec/cbor/streams_annotation.hpp"
#include "primitives/address/address.hpp"
#include "primitives/address/address_codec.hpp"
#include "vm/actor/actor_method.hpp"
#include "vm/actor/builtin/miner/types.hpp"

namespace fc::vm::actor::builtin::miner {
  constexpr MethodNumber kGetControlAddresses{2};

  constexpr MethodNumber kSubmitElectionPoStMethodNumber{20};

  outcome::result<InvocationOutput> constructor(const Actor &actor,
                                                Runtime &runtime,
                                                const MethodParams &params);

  outcome::result<InvocationOutput> controlAdresses(const Actor &actor,
                                                    Runtime &runtime,
                                                    const MethodParams &params);

  extern const ActorExports exports;
}  // namespace fc::vm::actor::builtin::miner

#endif  // CPP_FILECOIN_CORE_VM_ACTOR_BUILTIN_MINER_MINER_ACTOR_HPP
