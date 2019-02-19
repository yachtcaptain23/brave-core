/* Copyright (c) 2019 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef BRAVE_COMPONENTS_BRAVE_SYNC_BRAVE_SYNC_SCHEDULER_IMPL_H_
#define BRAVE_COMPONENTS_BRAVE_SYNC_BRAVE_SYNC_SCHEDULER_IMPL_H_

#include <map>
#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "components/sync/engine_impl/sync_scheduler.h"

namespace brave_sync {

class BraveSyncSchedulerImpl : public syncer::SyncScheduler {
 public:
  explicit BraveSyncSchedulerImpl(const std::string& name);

  // Calls Stop().
  ~BraveSyncSchedulerImpl() override;

  void Start(Mode mode, base::Time last_poll_time) override;
  void ScheduleConfiguration(
      const syncer::ConfigurationParams& params) override;
  void ScheduleClearServerData(const syncer::ClearParams& params) override;
  void Stop() override;
  void ScheduleLocalNudge(syncer::ModelTypeSet types,
                          const base::Location& nudge_location) override;
  void ScheduleLocalRefreshRequest(
      syncer::ModelTypeSet types,
      const base::Location& nudge_location) override;
  void ScheduleInvalidationNudge(
      syncer::ModelType type,
      std::unique_ptr<syncer::InvalidationInterface> invalidation,
      const base::Location& nudge_location) override;
  void ScheduleInitialSyncNudge(syncer::ModelType model_type) override;
  void SetNotificationsEnabled(bool notifications_enabled) override;

  void OnCredentialsUpdated() override;
  void OnConnectionStatusChange(network::mojom::ConnectionType type) override;

  // SyncCycle::Delegate implementation.
  void OnThrottled(const base::TimeDelta& throttle_duration) override {}
  void OnTypesThrottled(syncer::ModelTypeSet types,
                        const base::TimeDelta& throttle_duration) override {}
  void OnTypesBackedOff(syncer::ModelTypeSet types) override {}
  bool IsAnyThrottleOrBackoff() override;
  void OnReceivedShortPollIntervalUpdate(
      const base::TimeDelta& new_interval) override {}
  void OnReceivedLongPollIntervalUpdate(
      const base::TimeDelta& new_interval) override {}
  void OnReceivedCustomNudgeDelays(
      const std::map<syncer::ModelType,
      base::TimeDelta>& nudge_delays) override {}
  void OnReceivedClientInvalidationHintBufferSize(int size) override {}
  void OnSyncProtocolError(
      const syncer::SyncProtocolError& sync_protocol_error) override {}
  void OnReceivedGuRetryDelay(const base::TimeDelta& delay) override {}
  void OnReceivedMigrationRequest(syncer::ModelTypeSet types) override {}

 private:
  // Set in Start(), unset in Stop().
  bool started_;

  // Used for logging.
  const std::string name_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<BraveSyncSchedulerImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(BraveSyncSchedulerImpl);
};

}   // namespace brave_sync

#endif    // BRAVE_COMPONENTS_BRAVE_SYNC_BRAVE_SYNC_SCHEDULER_IMPL_H_
