/**
 * Copyright Soramitsu Co., Ltd. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef CPP_FILECOIN_CORE_VM_ACTOR_BUILTIN_MINER_TYPES_HPP
#define CPP_FILECOIN_CORE_VM_ACTOR_BUILTIN_MINER_TYPES_HPP

#include <libp2p/multi/uvarint.hpp>

#include "primitives/address/address_codec.hpp"
#include "primitives/big_int.hpp"
#include "primitives/chain_epoch/chain_epoch.hpp"
#include "primitives/rle_bitset/rle_bitset.hpp"
#include "proofs/sector.hpp"

namespace fc::vm::actor::builtin::miner {
  using common::Buffer;
  using libp2p::multi::UVarint;
  using primitives::BigInt;
  using primitives::ChainEpoch;
  using primitives::EpochDuration;
  using primitives::RleBitset;
  using primitives::address::Address;
  using proofs::sector::OnChainPoStVerifyInfo;
  using proofs::sector::SealProof;

  using PeerId = std::string;

  struct PoStState {
    uint64_t proving_period_start;
    uint64_t num_consecutive_failures;
  };

  struct SectorPreCommitInfo {
    uint64_t sector;
    CID sealed_cid;
    ChainEpoch seal_epoch;
    std::vector<uint64_t> deal_ids;
    ChainEpoch expiration;
  };

  struct SectorPreCommitOnChainInfo {
    SectorPreCommitInfo info;
    BigInt precommit_deposit;
    ChainEpoch precommit_epoch;
  };

  struct SectorOnChainInfo {
    SectorPreCommitInfo info;
    ChainEpoch activation_epoch;
    BigInt deal_weight;
    BigInt pledge_requirement;
    ChainEpoch declared_fault_epoch;
    ChainEpoch declared_fault_duration;
  };

  struct WorkerKeyChange {
    Address new_worker;
    uint64_t effective_at;
  };

  struct MinerInfo {
    Address owner;
    Address worker;
    boost::optional<WorkerKeyChange> pending_worker_key;
    PeerId peer_id;
    uint64_t sector_size;
  };

  struct MinerActorState {
    CID precommitted_sectors;
    CID sectors;
    RleBitset fault_set;
    CID proving_set;
    MinerInfo info;
    PoStState post_state;
  };

  enum class CronEventType {
    WindowedPoStExpiration,
    WorkerKeyChange,
    PreCommitExpiry,
    SectorExpiry,
    TempFault,
  };

  struct CronEventPayload {
    CronEventType event_type;
    boost::optional<RleBitset> sectors;
  };

  struct ConstructorParams {
    Address owner;
    Address worker;
    uint64_t sector_size;
    PeerId peer_id;
  };

  struct GetControlAddressesResult {
    Address owner;
    Address worker;
  };

  struct ChangeWorkerAddressParams {
    Address new_worker;
  };

  struct ChangePeerIDParams {
    PeerId new_id;
  };

  using SubmitWindowedPoStParams = OnChainPoStVerifyInfo;

  struct ProveCommitSectorParams {
    uint64_t sector;
    SealProof proof;
  };

  struct ExtendSectorExpirationParams {
    uint64_t sector;
    ChainEpoch new_expiration;
  };

  struct TerminateSectorsParams {
    boost::optional<RleBitset> sectors;
  };

  struct DeclareTemporaryFaultsParams {
    RleBitset sectors;
    EpochDuration duration;
  };

  struct OnDeferredCronEventParams {
    Buffer callback_payload;
  };

  CBOR_TUPLE(PoStState, proving_period_start, num_consecutive_failures)

  CBOR_TUPLE(
      SectorPreCommitInfo, sector, sealed_cid, seal_epoch, deal_ids, expiration)

  CBOR_TUPLE(SectorPreCommitOnChainInfo,
             info,
             precommit_deposit,
             precommit_epoch)

  CBOR_TUPLE(SectorOnChainInfo,
             info,
             activation_epoch,
             deal_weight,
             pledge_requirement,
             declared_fault_epoch,
             declared_fault_duration)

  CBOR_TUPLE(WorkerKeyChange, new_worker, effective_at)

  CBOR_TUPLE(MinerInfo, owner, worker, pending_worker_key, peer_id, sector_size)

  CBOR_TUPLE(MinerActorState,
             precommitted_sectors,
             sectors,
             fault_set,
             proving_set,
             info,
             post_state)

  CBOR_TUPLE(CronEventPayload, event_type, sectors)

  CBOR_TUPLE(ConstructorParams, owner, worker, sector_size, peer_id)

  CBOR_TUPLE(GetControlAddressesResult, owner, worker)

  CBOR_TUPLE(ChangeWorkerAddressParams, new_worker)

  CBOR_TUPLE(ChangePeerIDParams, new_id)

  CBOR_TUPLE(ProveCommitSectorParams, sector, proof)

  CBOR_TUPLE(ExtendSectorExpirationParams, sector, new_expiration)

  CBOR_TUPLE(TerminateSectorsParams, sectors)

  CBOR_TUPLE(DeclareTemporaryFaultsParams, sectors, duration)

  CBOR_TUPLE(OnDeferredCronEventParams, callback_payload)
}  // namespace fc::vm::actor::builtin::miner

#endif  // CPP_FILECOIN_CORE_VM_ACTOR_BUILTIN_MINER_TYPES_HPP
