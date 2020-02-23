/**
 * Copyright Soramitsu Co., Ltd. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "vm/actor/builtin/miner/miner_actor.hpp"

#include "primitives/chain_epoch/chain_epoch_codec.hpp"
#include "storage/amt/amt.hpp"
#include "storage/hamt/hamt.hpp"
#include "vm/actor/builtin/account/account_actor.hpp"
#include "vm/actor/builtin/market/actor.hpp"
#include "vm/actor/builtin/miner/policy.hpp"
#include "vm/actor/builtin/storage_power/storage_power_actor_export.hpp"
#include "vm/exit_code/exit_code.hpp"

namespace fc::vm::actor::builtin::miner {
  using market::ComputeDataCommitmentParams;
  using market::kComputeDataCommitmentMethodNumber;
  using market::kVerifyDealsOnSectorProveCommitMethodNumber;
  using market::VerifyDealsOnSectorProveCommitParams;
  using primitives::DealWeight;
  using primitives::kChainEpochUndefined;
  using primitives::SectorSize;
  using primitives::TokenAmount;
  using primitives::address::Protocol;
  using primitives::chain_epoch::uvarintKey;
  using proofs::sector::OnChainSealVerifyInfo;
  using proofs::sector::PoStCandidate;
  using proofs::sector::SectorInfo;
  using runtime::DomainSeparationTag;
  using storage::amt::Amt;
  using storage::hamt::Hamt;
  using storage_power::EnrollCronEventParams;
  using storage_power::kEnrollCronEventMethodNumber;
  using storage_power::kOnMinerSurprisePoStSuccessMethodNumber;
  using storage_power::kOnSectorModifyWeightDescMethodNumber;
  using storage_power::kOnSectorProveCommitMethodNumber;
  using storage_power::OnSectorModifyWeightDescParams;
  using storage_power::OnSectorProveCommitParams;
  using storage_power::SectorStorageWeightDesc;

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
      OUTCOME_TRY(key,
                  runtime.sendR<Address>(
                      id, account::kPubkeyAddressMethodNumber, {}, 0));
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
    OUTCOME_TRY(runtime.sendP(kStoragePowerAddress,
                              kEnrollCronEventMethodNumber,
                              EnrollCronEventParams{
                                  event_epoch,
                                  Buffer{payload2},
                              },
                              0));
    return outcome::success();
  }

  bool hasDuplicateTickets(const std::vector<PoStCandidate> &candidates) {
    std::set<int64_t> set;
    for (auto &candidate : candidates) {
      if (!set.insert(candidate.challenge_index).second) {
        return true;
      }
    }
    return false;
  }

  outcome::result<void> verifyWidowedPost(
      Runtime &runtime,
      const MinerActorState &state,
      const SubmitWindowedPoStParams &params) {
    if (hasDuplicateTickets(params.candidates)) {
      return VMExitCode::MINER_ACTOR_ILLEGAL_ARGUMENT;
    }
    if (params.candidates.size() != kNumWindowedPoStSectors) {
      return VMExitCode::MINER_ACTOR_ILLEGAL_ARGUMENT;
    }

    std::vector<SectorInfo> sectors;
    OUTCOME_TRY(
        Amt(runtime.getIpfsDatastore(), state.proving_set)
            .visit([&](auto, auto &raw) -> outcome::result<void> {
              OUTCOME_TRY(sector, codec::cbor::decode<SectorOnChainInfo>(raw));
              if (sector.declared_fault_epoch != kChainEpochUndefined
                  || sector.declared_fault_duration != kChainEpochUndefined) {
                return VMExitCode::MINER_ACTOR_ILLEGAL_STATE;
              }
              if (state.fault_set.find(sector.info.sector)
                  == state.fault_set.end()) {
                sectors.push_back({
                    sector.info.sector,
                    sector.info.sealed_cid,
                });
              }
              return outcome::success();
            }));

    // TODO: ensure message.to is id-address
    OUTCOME_TRY(seed, codec::cbor::encode(runtime.getMessage().get().to));
    OUTCOME_TRY(
        verified,
        runtime.verifyPoSt(
            state.info.sector_size,
            {
                runtime.getRandomness(DomainSeparationTag::PoStDST,
                                      state.post_state.proving_period_start,
                                      Buffer{seed}),
                {},
                params.candidates,
                params.proofs,
                sectors,
            }));
    if (!verified) {
      return VMExitCode::MINER_ACTOR_ILLEGAL_ARGUMENT;
    }
    return outcome::success();
  }

  outcome::result<void> confirmPaymentAndRefundChange(Runtime &runtime,
                                                      BigInt expected) {
    BigInt extra = expected - runtime.getValueReceived();
    if (extra < 0) {
      return VMExitCode::MINER_ACTOR_INSUFFICIENT_FUNDS;
    }
    if (extra > 0) {
      OUTCOME_TRY(runtime.sendFunds(runtime.getImmediateCaller(), extra));
    }
    return outcome::success();
  }

  bool inChallengeWindow(const MinerActorState &state, Runtime &runtime) {
    return runtime.getCurrentEpoch() > state.post_state.proving_period_start;
  }

  outcome::result<EpochDuration> maxSealDuration(RegisteredProof type) {
    // TODO: seal types
    return VMExitCode::MINER_ACTOR_ILLEGAL_ARGUMENT;
  }

  outcome::result<void> verifySeal(Runtime &runtime,
                                   SectorSize sector_size,
                                   const OnChainSealVerifyInfo &info) {
    OUTCOME_TRY(duration, maxSealDuration(info.registered_proof));
    if (info.seal_rand_epoch
        < runtime.getCurrentEpoch() - kChainFinalityish - duration) {
      return VMExitCode::MINER_ACTOR_ILLEGAL_ARGUMENT;
    }

    OUTCOME_TRY(comm_d,
                runtime.sendPR<CID>(kStorageMarketAddress,
                                    kComputeDataCommitmentMethodNumber,
                                    ComputeDataCommitmentParams{
                                        .sector_size = sector_size,
                                        .deals = info.deals,
                                    },
                                    0));

    // TODO: ensure message.to is id-address
    OUTCOME_TRY(runtime.verifySeal(
        sector_size,
        {
            .sector =
                {
                    .miner = runtime.getMessage().get().to.getId(),
                    .sector = info.sector,
                },
            .info = info,
            .randomness = runtime.getRandomness(
                DomainSeparationTag::SealRandomness, info.seal_rand_epoch),
            .interactive_randomness = runtime.getRandomness(
                DomainSeparationTag::InteractiveSealChallengeSeed,
                info.interactive_epoch),
            .unsealed_cid = comm_d,
        }));
    return outcome::success();
  }

  SectorStorageWeightDesc asStorageWeightDesc(SectorSize sector_size,
                                              const SectorOnChainInfo &sector) {
    return {
        .sector_size = sector_size,
        .duration = sector.info.expiration - sector.activation_epoch,
        .deal_weight = sector.deal_weight,
    };
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

  ACTOR_METHOD(submitWindowedPoSt) {
    OUTCOME_TRY(params2, decodeActorParams<SubmitWindowedPoStParams>(params));
    OUTCOME_TRY(state, runtime.getCurrentActorStateCbor<MinerActorState>());
    if (runtime.getImmediateCaller() != state.info.worker) {
      return VMExitCode::MINER_ACTOR_WRONG_CALLER;
    }
    if (runtime.getCurrentEpoch() > state.post_state.proving_period_start
                                        + kWindowedPostChallengeDuration) {
      return VMExitCode::MINER_ACTOR_POST_TOO_LATE;
    }
    if (runtime.getCurrentEpoch() <= state.post_state.proving_period_start) {
      return VMExitCode::MINER_ACTOR_POST_TOO_EARLY;
    }
    OUTCOME_TRY(verifyWidowedPost(runtime, state, params2));
    state.post_state.num_consecutive_failures = 0;
    state.post_state.proving_period_start += kProvingPeriod;
    state.proving_set = state.sectors;
    OUTCOME_TRY(runtime.commitState(state));
    OUTCOME_TRY(runtime.send(
        kStoragePowerAddress, kOnMinerSurprisePoStSuccessMethodNumber, {}, 0));
    return outcome::success();
  }

  ACTOR_METHOD(onDeleteMiner) {
    if (runtime.getImmediateCaller() != kStoragePowerAddress) {
      return VMExitCode::MINER_ACTOR_WRONG_CALLER;
    }
    OUTCOME_TRY(runtime.deleteActor());
    return outcome::success();
  }

  ACTOR_METHOD(preCommitSector) {
    OUTCOME_TRY(params2, decodeActorParams<PreCommitSectorParams>(params));
    OUTCOME_TRY(state, runtime.getCurrentActorStateCbor<MinerActorState>());
    if (runtime.getImmediateCaller() != state.info.worker) {
      return VMExitCode::MINER_ACTOR_WRONG_CALLER;
    }

    OUTCOME_TRY(already_commited,
                Amt(runtime.getIpfsDatastore(), state.sectors)
                    .contains(params2.sector));
    if (already_commited) {
      return VMExitCode::MINER_ACTOR_ILLEGAL_ARGUMENT;
    }

    auto deposit = precommitDeposit(
        state.info.sector_size, params2.expiration - runtime.getCurrentEpoch());
    OUTCOME_TRY(confirmPaymentAndRefundChange(runtime, deposit));

    Hamt hamt_precommitted_sectors{runtime.getIpfsDatastore(),
                                   state.precommitted_sectors};
    OUTCOME_TRY(hamt_precommitted_sectors.setCbor(uvarintKey(params2.sector),
                                                  SectorPreCommitOnChainInfo{
                                                      params2,
                                                      deposit,
                                                      runtime.getCurrentEpoch(),
                                                  }));
    OUTCOME_TRY(hamt_precommitted_sectors.flush());
    state.precommitted_sectors = hamt_precommitted_sectors.cid();
    OUTCOME_TRY(runtime.commitState(state));

    if (params2.expiration <= runtime.getCurrentEpoch()) {
      return VMExitCode::MINER_ACTOR_ILLEGAL_ARGUMENT;
    }

    OUTCOME_TRY(duration, maxSealDuration(params2.registered_proof));
    OUTCOME_TRY(enrollCronEvent(
        runtime,
        runtime.getCurrentEpoch() + duration + 1,
        {CronEventType::PreCommitExpiry, RleBitset{params2.sector}}));

    return outcome::success();
  }

  ACTOR_METHOD(proveCommitSector) {
    OUTCOME_TRY(params2, decodeActorParams<ProveCommitSectorParams>(params));
    OUTCOME_TRY(state, runtime.getCurrentActorStateCbor<MinerActorState>());
    if (runtime.getImmediateCaller() != state.info.worker) {
      return VMExitCode::MINER_ACTOR_WRONG_CALLER;
    }
    auto ipld = runtime.getIpfsDatastore();
    auto sector = params2.sector;

    OUTCOME_TRY(precommit,
                Hamt(ipld, state.precommitted_sectors)
                    .getCbor<SectorPreCommitOnChainInfo>(uvarintKey(sector)));

    OUTCOME_TRY(verifySeal(runtime,
                           state.info.sector_size,
                           {
                               .sealed_cid = precommit.info.sealed_cid,
                               .interactive_epoch = precommit.precommit_epoch
                                                    + kPreCommitChallengeDelay,
                               .registered_proof = {},
                               .proof = params2.proof,
                               .deals = precommit.info.deal_ids,
                               .sector = precommit.info.sector,
                               .seal_rand_epoch = precommit.info.seal_epoch,
                           }));

    OUTCOME_TRY(deal_weight,
                runtime.sendPR<DealWeight>(
                    kStorageMarketAddress,
                    kVerifyDealsOnSectorProveCommitMethodNumber,
                    VerifyDealsOnSectorProveCommitParams{
                        .deals = precommit.info.deal_ids,
                        .sector_expiry = precommit.info.expiration,
                    },
                    0));

    OUTCOME_TRY(pledge,
                runtime.sendPR<TokenAmount>(
                    kStoragePowerAddress,
                    kOnSectorProveCommitMethodNumber,
                    OnSectorProveCommitParams{
                        .weight =
                            {
                                .sector_size = state.info.sector_size,
                                .duration = precommit.info.expiration
                                            - runtime.getCurrentEpoch(),
                                .deal_weight = deal_weight,
                            }},
                    0));

    Amt amt_sectors{ipld, state.sectors};
    OUTCOME_TRY(sectors, amt_sectors.count());
    OUTCOME_TRY(
        amt_sectors.setCbor(precommit.info.sector,
                            SectorOnChainInfo{
                                .info = precommit.info,
                                .activation_epoch = runtime.getCurrentEpoch(),
                                .deal_weight = deal_weight,
                                .pledge_requirement = pledge,
                                .declared_fault_epoch = {},
                                .declared_fault_duration = {},
                            }));
    OUTCOME_TRY(amt_sectors.flush());
    state.sectors = amt_sectors.cid();

    Hamt hamt_precommitted_sectors{ipld, state.precommitted_sectors};
    OUTCOME_TRY(hamt_precommitted_sectors.remove(uvarintKey(sector)));
    OUTCOME_TRY(hamt_precommitted_sectors.flush());
    state.precommitted_sectors = hamt_precommitted_sectors.cid();

    if (sectors == 1) {
      state.post_state.proving_period_start =
          runtime.getCurrentEpoch() + kProvingPeriod;
    }

    if (!inChallengeWindow(state, runtime)) {
      state.proving_set = state.sectors;
    }

    OUTCOME_TRY(runtime.commitState(state));

    OUTCOME_TRY(enrollCronEvent(runtime,
                                precommit.info.expiration,
                                {
                                    .event_type = CronEventType::SectorExpiry,
                                    .sectors = RleBitset{sector},
                                }));

    if (sectors == 1) {
      OUTCOME_TRY(enrollCronEvent(
          runtime,
          state.post_state.proving_period_start
              + kWindowedPostChallengeDuration,
          {
              .event_type = CronEventType::WindowedPoStExpiration,
              .sectors = boost::none,
          }));
    }

    OUTCOME_TRY(
        runtime.sendFunds(state.info.worker, precommit.precommit_deposit));
    return outcome::success();
  }

  ACTOR_METHOD(extendSectorExpiration) {
    OUTCOME_TRY(params2,
                decodeActorParams<ExtendSectorExpirationParams>(params));
    OUTCOME_TRY(state, runtime.getCurrentActorStateCbor<MinerActorState>());
    if (runtime.getImmediateCaller() != state.info.worker) {
      return VMExitCode::MINER_ACTOR_WRONG_CALLER;
    }
    Amt amt_sectors{runtime.getIpfsDatastore(), state.sectors};

    OUTCOME_TRY(sector, amt_sectors.getCbor<SectorOnChainInfo>(params2.sector));

    auto prev_weight{asStorageWeightDesc(state.info.sector_size, sector)};
    auto extension = params2.new_expiration - sector.info.expiration;
    if (extension < 0) {
      return VMExitCode::MINER_ACTOR_ILLEGAL_ARGUMENT;
    }
    auto new_weight{prev_weight};
    new_weight.duration = prev_weight.duration + extension;

    OUTCOME_TRY(new_pledge,
                runtime.sendPR<TokenAmount>(
                    kStoragePowerAddress,
                    kOnSectorModifyWeightDescMethodNumber,
                    OnSectorModifyWeightDescParams{
                        .prev_weight = prev_weight,
                        .prev_pledge = sector.pledge_requirement,
                        .new_weight = new_weight,
                    },
                    0));

    sector.info.expiration = params2.new_expiration;
    sector.pledge_requirement = new_pledge;
    OUTCOME_TRY(amt_sectors.setCbor(sector.info.sector, sector));
    OUTCOME_TRY(amt_sectors.flush());

    state.sectors = amt_sectors.cid();
    OUTCOME_TRY(runtime.commitState(state));
    return outcome::success();
  }

  const ActorExports exports = {
      {kConstructorMethodNumber, ActorMethod(constructor)},
      {kGetControlAddressesMethodNumber, ActorMethod(controlAdresses)},
      {kChangeWorkerAddressMethodNumber, ActorMethod(changeWorkerAddress)},
      {kChangePeerIdMethodNumber, ActorMethod(changePeerId)},
      {kSubmitWindowedPoStMethodNumber, ActorMethod(submitWindowedPoSt)},
      {kOnDeleteMinerMethodNumber, ActorMethod(onDeleteMiner)},
      {kPreCommitSectorMethodNumber, ActorMethod(preCommitSector)},
      {kProveCommitSectorMethodNumber, ActorMethod(proveCommitSector)},
      {kExtendSectorExpirationMethodNumber,
       ActorMethod(extendSectorExpiration)},
  };
}  // namespace fc::vm::actor::builtin::miner
