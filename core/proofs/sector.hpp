/**
 * Copyright Soramitsu Co., Ltd. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef CPP_FILECOIN_CORE_PROOFS_SECTOR_HPP
#define CPP_FILECOIN_CORE_PROOFS_SECTOR_HPP

#include "proofs/proofs.hpp"

namespace fc::proofs::sector {
  using common::Buffer;

  using RegisteredProof = int64_t;

  struct SealProof {
    Buffer proof;
  };

  struct PoStProof {
    Buffer proof;
  };

  struct SectorId {
    uint64_t miner;
    uint64_t sector;
  };

  struct PrivatePoStCandidateProof {
    RegisteredProof registered_proof;
    Buffer externalized;
  };

  struct PoStCandidate {
    RegisteredProof registered_proof;
    Ticket partial_ticket;
    PrivatePoStCandidateProof private_proof;
    SectorId sector;
    int64_t challenge_index;
  };

  struct OnChainPoStVerifyInfo {
    RegisteredProof proof_type;
    std::vector<PoStCandidate> candidates;
    std::vector<PoStProof> proofs;
  };

  CBOR_TUPLE(SealProof, proof)

  CBOR_TUPLE(PoStProof, proof)

  CBOR_TUPLE(SectorId, miner, sector)

  CBOR_TUPLE(PrivatePoStCandidateProof, registered_proof, externalized)

  CBOR_TUPLE(PoStCandidate, registered_proof, partial_ticket, private_proof, sector, challenge_index)

  CBOR_TUPLE(OnChainPoStVerifyInfo, proof_type, candidates, proofs)
}  // namespace fc::proofs::sector

#endif  // CPP_FILECOIN_CORE_PROOFS_SECTOR_HPP
