/**
 * Copyright Soramitsu Co., Ltd. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "storage/ipld/impl/ipld_block_impl.hpp"

#include <libp2p/crypto/sha/sha256.hpp>
#include <libp2p/multi/multihash.hpp>
#include "crypto/blake2/blake2b160.hpp"

namespace fc::storage::ipld {
  using crypto::blake2b::Blake2b256Hash;
  using libp2p::common::Hash256;
  using libp2p::multi::Multihash;

  std::map<HashType, IPLDBlockImpl::HashMethod> IPLDBlockImpl::hash_methods_{
      {HashType::sha256, IPLDBlockImpl::hash_sha2_256},
      {HashType::blake2b_256, IPLDBlockImpl::hash_blake2b_256}};

    IPLDBlockImpl::IPLDBlockImpl(CID::Version version,
                               HashType hash_type,
                               ContentType content_type)
      : cid_version_{version},
        cid_hash_type_{hash_type},
        content_type_{content_type} {}

  const CID &IPLDBlockImpl::getCID() const {
    if (!cid_) {
      HashMethod hash_method = hash_methods_.at(cid_hash_type_);
      const common::Buffer &raw_data = getRawBytes();
      std::vector<uint8_t> digest = std::invoke(hash_method, raw_data);
      auto multi_hash = Multihash::create(cid_hash_type_, digest);
      CID id{cid_version_, content_type_, std::move(multi_hash.value())};
      cid_ = std::move(id);
    }
    return cid_.value();
  }

    const common::Buffer &IPLDBlockImpl::getRawBytes() const {
      if (!raw_bytes_) {
        raw_bytes_ = getBlockContent();
      }
      return raw_bytes_.value();
    }

    void IPLDBlockImpl::clearCache() const {
      cid_ = boost::none;
      raw_bytes_ = boost::none;
    }

  std::vector<uint8_t> IPLDBlockImpl::hash_sha2_256(
      const common::Buffer &data) {
    Hash256 digest = libp2p::crypto::sha256(data.toVector());
    return {std::make_move_iterator(digest.begin()),
            std::make_move_iterator((digest.end()))};
  }

  std::vector<uint8_t> IPLDBlockImpl::hash_blake2b_256(
      const common::Buffer &data) {
    Blake2b256Hash digest = crypto::blake2b::blake2b_256(data.toVector());
    return {std::make_move_iterator(digest.begin()),
            std::make_move_iterator((digest.end()))};
  }
}  // namespace fc::storage::ipld
