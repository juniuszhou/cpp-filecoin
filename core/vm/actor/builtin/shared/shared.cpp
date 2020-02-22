/**
 * Copyright Soramitsu Co., Ltd. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "vm/actor/builtin/shared/shared.hpp"
#include "adt/balance_table_hamt.hpp"
#include "codec/cbor/cbor.hpp"

namespace fc::vm::actor::builtin {

  using adt::TokenAmount;
  using codec::cbor::decode;
  using miner::kGetControlAddressesMethodNumber;
  using power::Address;
  using runtime::Runtime;

  fc::outcome::result<GetControlAddressesReturn> requestMinerControlAddress(
      Runtime &runtime, const Address &miner) {
    OUTCOME_TRY(
        result,
        runtime.send(
            miner, kGetControlAddressesMethodNumber, {}, TokenAmount{0}));
    OUTCOME_TRY(
        addresses,
        decode<GetControlAddressesReturn>(result.return_value.toVector()));
    return std::move(addresses);
  }

}  // namespace fc::vm::actor::builtin
