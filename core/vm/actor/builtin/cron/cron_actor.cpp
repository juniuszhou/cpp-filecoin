/**
 * Copyright Soramitsu Co., Ltd. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "vm/actor/builtin/cron/cron_actor.hpp"
#include "vm/actor/builtin/storage_power/storage_power_actor_export.hpp"

namespace fc::vm::actor::builtin::cron {
  /**
   * Entries is a set of actors (and corresponding methods) to call during
   * EpochTick
   */
  std::vector<CronTableEntry> entries = {
      {kStoragePowerAddress, {storage_power::kOnEpochTickEndMethodNumber}}};

  outcome::result<InvocationOutput> epochTick(const Actor &actor,
                                              Runtime &runtime,
                                              const MethodParams &params) {
    if ((runtime.getMessage().get().from != kCronAddress)) {
      return VMExitCode::CRON_ACTOR_WRONG_CALL;
    }

    for (const auto &entry : entries) {
      OUTCOME_TRY(runtime.send(entry.to_addr, entry.method_num, {}, BigInt(0)));
    }
    return outcome::success();
  }

  const ActorExports exports = {
      {kEpochTickMethodNumber, ActorMethod(epochTick)},
  };
}  // namespace fc::vm::actor::builtin::cron
