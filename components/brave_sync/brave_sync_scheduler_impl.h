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
#include "components/sync/engine_impl/cycle/nudge_tracker.h"
#include "components/sync/engine_impl/cycle/sync_cycle_context.h"
#include "components/sync/engine_impl/sync_scheduler.h"
#include "components/sync/engine_impl/syncer.h"

namespace brave_sync {

class BraveSyncSchedulerImpl : public syncer::SyncScheduler {
 public:
  BraveSyncSchedulerImpl(const std::string& name,
                         syncer::SyncCycleContext* context,
                         syncer::Syncer* syncer);

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
      const base::TimeDelta& new_interval) override;
  void OnReceivedLongPollIntervalUpdate(
      const base::TimeDelta& new_interval) override;
  void OnReceivedCustomNudgeDelays(
      const std::map<syncer::ModelType,
      base::TimeDelta>& nudge_delays) override {}
  void OnReceivedClientInvalidationHintBufferSize(int size) override {}
  void OnSyncProtocolError(
      const syncer::SyncProtocolError& sync_protocol_error) override {}
  void OnReceivedGuRetryDelay(const base::TimeDelta& delay) override {}
  void OnReceivedMigrationRequest(syncer::ModelTypeSet types) override {}

 private:
  static const char* GetModeString(Mode mode);

  // Invoke the syncer to perform a nudge job.
  void DoNudgeSyncCycleJob();

  // Invoke the syncer to perform a configuration job.
  void DoConfigurationSyncCycleJob();

  void DoClearServerDataSyncCycleJob();

  // Invoke the Syncer to perform a poll job.
  void DoPollSyncCycleJob();

  // Helper function to calculate poll interval.
  base::TimeDelta GetPollInterval();

  // Determines if we're allowed to contact the server right now.
  bool CanRunNudgeJobNow();

  // At the moment TrySyncCycleJob just posts call to TrySyncCycleJobImpl on
  // current thread. In the future it will request access token here.
  void TrySyncCycleJob();
  void TrySyncCycleJobImpl();

  // Returns the set of types that are enabled and not currently throttled and
  // backed off.
  syncer::ModelTypeSet GetEnabledAndUnblockedTypes();

  // Called as we are started to broadcast an initial cycle snapshot
  // containing data like initial_sync_ended.  Important when the client starts
  // up and does not need to perform an initial sync.
  void SendInitialSnapshot();

  // Computes the last poll time the system should assume on start-up.
  static base::Time ComputeLastPollOnStart(base::Time last_poll,
                                           base::TimeDelta poll_interval,
                                           base::Time now);

  // Used for logging.
  const std::string name_;

  // Set in Start(), unset in Stop().
  bool started_;

  // Modifiable versions of kDefaultLongPollIntervalSeconds which can be
  // updated by the server.
  base::TimeDelta syncer_short_poll_interval_seconds_;
  base::TimeDelta syncer_long_poll_interval_seconds_;

  // The mode of operation.
  Mode mode_;

  // Storage for variables related to an in-progress configure request.  Note
  // that (mode_ != CONFIGURATION_MODE) \implies !pending_configure_params_.
  std::unique_ptr<syncer::ConfigurationParams> pending_configure_params_;

  // Keeps track of work that the syncer needs to handle.
  syncer::NudgeTracker nudge_tracker_;

  // Invoked to run through the sync cycle.
  std::unique_ptr<syncer::Syncer> syncer_;

  syncer::SyncCycleContext* cycle_context_;

  // TryJob might get called for multiple reasons. It should only call
  // DoPollSyncCycleJob after some time since the last attempt.
  // last_poll_reset_ keeps track of when was last attempt.
  base::TimeTicks last_poll_reset_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<BraveSyncSchedulerImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(BraveSyncSchedulerImpl);
};

}   // namespace brave_sync

#endif    // BRAVE_COMPONENTS_BRAVE_SYNC_BRAVE_SYNC_SCHEDULER_IMPL_H_
