/**
 * Copyright Soramitsu Co., Ltd. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "vm/actor/builtin/miner/miner_actor.hpp"

#include "storage/amt/amt.hpp"
#include "storage/hamt/hamt.hpp"
#include "vm/actor/builtin/account/account_actor.hpp"
#include "vm/exit_code/exit_code.hpp"

namespace fc::vm::actor::builtin::miner {
  using primitives::kChainEpochUndefined;
  using primitives::address::Protocol;
  using storage::amt::Amt;
  using storage::hamt::Hamt;

  outcome::result<Address> resolveOwnerAddress(Runtime &runtime,
                                               const Address &address) {
    OUTCOME_TRY(id, runtime.resolveAddress(address));
    OUTCOME_TRY(code, runtime.getActorCodeID(id));
    if (!isSignableActor(code)) {
      return VMExitCode::MINER_ACTOR_OWNER_NOT_SIGNABLE;
    }
    return std::move(id);
  }

  outcome::result<Address> resolveWorkerAddress(Runtime &runtime,
                                                const Address &address) {
    OUTCOME_TRY(id, runtime.resolveAddress(address));
    OUTCOME_TRY(code, runtime.getActorCodeID(id));
    if (code != kAccountCodeCid) {
      return VMExitCode::MINER_ACTOR_MINER_NOT_ACCOUNT;
    }
    if (address.getProtocol() != Protocol::BLS) {
      OUTCOME_TRY(key_raw,
                  runtime.send(id, account::kPubkeyAddressMethodNumber, {}, 0));
      OUTCOME_TRY(key, codec::cbor::decode<Address>(key_raw.return_value));
      if (key.getProtocol() != Protocol::BLS) {
        return VMExitCode::MINER_ACTOR_MINER_NOT_BLS;
      }
    }
    return std::move(id);
  }

  ACTOR_METHOD(constructor) {
    if (runtime.getImmediateCaller() != kInitAddress) {
      return VMExitCode::MINER_ACTOR_WRONG_CALLER;
    }
    OUTCOME_TRY(params2, decodeActorParams<ConstructorParams>(params));
    auto ipld = runtime.getIpfsDatastore();
    OUTCOME_TRY(empty_amt, Amt{ipld}.flush());
    OUTCOME_TRY(empty_hamt, Hamt{ipld}.flush());
    OUTCOME_TRY(owner, resolveOwnerAddress(runtime, params2.owner));
    OUTCOME_TRY(worker, resolveWorkerAddress(runtime, params2.worker));
    MinerActorState state{
        empty_hamt,
        empty_amt,
        {},
        empty_amt,
        {
            owner,
            worker,
            boost::none,
            params2.peer_id,
            params2.sector_size,
        },
        {
            kChainEpochUndefined,
            0,
        },
    };
    OUTCOME_TRY(runtime.commitState(state));
    return outcome::success();
  }

  ACTOR_METHOD(controlAdresses) {
    OUTCOME_TRY(state,
                runtime.getIpfsDatastore()->getCbor<MinerActorState>(
                    runtime.getCurrentActorState()));
    OUTCOME_TRY(result,
                codec::cbor::encode(GetControlAddressesReturn{
                    state.info.owner, state.info.worker}));
    return InvocationOutput{Buffer{result}};
  }

  const ActorExports exports = {
      {kConstructorMethodNumber, ActorMethod(constructor)},
      {kGetControlAddresses, ActorMethod(controlAdresses)},
  };
}  // namespace fc::vm::actor::builtin::miner
