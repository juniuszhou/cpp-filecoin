/**
 * Copyright Soramitsu Co., Ltd. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "vm/actor/builtin/miner/miner_actor.hpp"

#include "storage/amt/amt.hpp"
#include "storage/hamt/hamt.hpp"
#include "vm/actor/builtin/account/account_actor.hpp"
#include "vm/actor/builtin/miner/policy.hpp"
#include "vm/actor/builtin/storage_power/storage_power_actor_export.hpp"
#include "vm/exit_code/exit_code.hpp"

namespace fc::vm::actor::builtin::miner {
  using primitives::kChainEpochUndefined;
  using primitives::address::Protocol;
  using storage::amt::Amt;
  using storage::hamt::Hamt;
  using storage_power::EnrollCronEventParams;
  using storage_power::kEnrollCronEventMethodNumber;

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

  outcome::result<void> enrollCronEvent(Runtime &runtime,
                                        ChainEpoch event_epoch,
                                        const CronEventPayload &payload) {
    OUTCOME_TRY(payload2, codec::cbor::encode(payload));
    OUTCOME_TRY(params,
                codec::cbor::encode(EnrollCronEventParams{
                    event_epoch,
                    Buffer{payload2},
                }));
    OUTCOME_TRY(runtime.send(kStoragePowerAddress,
                             kEnrollCronEventMethodNumber,
                             MethodParams{params},
                             0));
    return outcome::success();
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
    OUTCOME_TRY(state, runtime.getCurrentActorStateCbor<MinerActorState>());
    OUTCOME_TRY(result,
                codec::cbor::encode(GetControlAddressesReturn{
                    state.info.owner, state.info.worker}));
    return InvocationOutput{Buffer{result}};
  }

  ACTOR_METHOD(changeWorkerAddress) {
    OUTCOME_TRY(params2, decodeActorParams<ChangeWorkerAddressParams>(params));
    OUTCOME_TRY(state, runtime.getCurrentActorStateCbor<MinerActorState>());
    if (runtime.getImmediateCaller() != state.info.owner) {
      return VMExitCode::MINER_ACTOR_WRONG_CALLER;
    }
    OUTCOME_TRY(worker, resolveWorkerAddress(runtime, params2.new_worker));
    auto effective_at = runtime.getCurrentEpoch() + kWorkerKeyChangeDelay;
    state.info.pending_worker_key = WorkerKeyChange{
        worker,
        effective_at,
    };
    OUTCOME_TRY(runtime.commitState(state));
    OUTCOME_TRY(enrollCronEvent(runtime,
                                effective_at,
                                {
                                    CronEventType::WorkerKeyChange,
                                    boost::none,
                                }));
    return outcome::success();
  }

  ACTOR_METHOD(changePeerId) {
    OUTCOME_TRY(params2, decodeActorParams<ChangePeerIdParams>(params));
    OUTCOME_TRY(state, runtime.getCurrentActorStateCbor<MinerActorState>());
    state.info.peer_id = params2.new_id;
    OUTCOME_TRY(runtime.commitState(state));
    return outcome::success();
  }

  const ActorExports exports = {
      {kConstructorMethodNumber, ActorMethod(constructor)},
      {kGetControlAddressesMethodNumber, ActorMethod(controlAdresses)},
      {kChangeWorkerAddressMethodNumber, ActorMethod(changeWorkerAddress)},
      {kChangePeerIdMethodNumber, ActorMethod(changePeerId)},
  };
}  // namespace fc::vm::actor::builtin::miner
