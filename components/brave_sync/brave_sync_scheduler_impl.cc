/* Copyright (c) 2019 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "brave/components/brave_sync/brave_sync_scheduler_impl.h"

#include <memory>
#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/rand_util.h"
#include "base/sequenced_task_runner.h"
#include "base/threading/platform_thread.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "components/sync/base/logging.h"
#include "components/sync/engine/sync_engine_switches.h"
#include "components/sync/protocol/proto_enum_conversions.h"
#include "components/sync/protocol/sync.pb.h"

using base::TimeDelta;
using base::TimeTicks;

namespace brave_sync {

namespace {

#define ENUM_CASE(x) \
  case x:            \
    return #x;       \
    break;

}  // namespace

using syncer::ConfigurationParams;
using syncer::ClearParams;
using syncer::SyncCycle;
using syncer::SyncCycleEvent;
using syncer::ProtocolTypes;
using syncer::ModelTypeSet;

// Helper macros to log with the syncer thread name; useful when there
// are multiple syncer threads involved.

#define SLOG(severity) LOG(severity) << name_ << ": "

#define SDVLOG(verbose_level) DVLOG(verbose_level) << name_ << ": "

#define SDVLOG_LOC(from_here, verbose_level) \
  DVLOG_LOC(from_here, verbose_level) << name_ << ": "

BraveSyncSchedulerImpl::BraveSyncSchedulerImpl(
    const std::string& name,
    syncer::SyncCycleContext* context,
    syncer::Syncer* syncer)
    : name_(name),
      started_(false),
      syncer_short_poll_interval_seconds_(context->short_poll_interval()),
      syncer_long_poll_interval_seconds_(context->long_poll_interval()),
      mode_(CONFIGURATION_MODE),
      syncer_(syncer),
      cycle_context_(context),
      weak_ptr_factory_(this) {}

BraveSyncSchedulerImpl::~BraveSyncSchedulerImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  Stop();
}

void BraveSyncSchedulerImpl::OnCredentialsUpdated() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void BraveSyncSchedulerImpl::OnConnectionStatusChange(
    network::mojom::ConnectionType type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void BraveSyncSchedulerImpl::Start(Mode mode, base::Time last_poll_time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::string thread_name = base::PlatformThread::GetName();
  if (thread_name.empty())
    thread_name = "<Main thread>";
  SDVLOG(2) << "Start called from thread " << thread_name << " with mode "
            << GetModeString(mode);
  if (!started_) {
    started_ = true;
    SendInitialSnapshot();
  }
  if (mode == CLEAR_SERVER_DATA_MODE) {
    DCHECK_EQ(mode_, CONFIGURATION_MODE);
  }
  Mode old_mode = mode_;
  mode_ = mode;
  base::Time now = base::Time::Now();

  // Only adjust the poll reset time if it was valid and in the past.
  if (!last_poll_time.is_null() && last_poll_time <= now) {
    // Convert from base::Time to base::TimeTicks. The reason we use Time
    // for persisting is that TimeTicks can stop making forward progress when
    // the machine is suspended. This implies that on resume the client might
    // actually have miss the real poll, unless the client is restarted.
    // Fixing that would require using an AlarmTimer though, which is only
    // supported on certain platforms.
    last_poll_reset_ =
        TimeTicks::Now() -
        (now - ComputeLastPollOnStart(last_poll_time, GetPollInterval(), now));
  }

  if (old_mode != mode_ && mode_ == NORMAL_MODE) {
    // We just got back to normal mode.  Let's try to run the work that was
    // queued up while we were configuring.

    AdjustPolling(UPDATE_INTERVAL);  // Will kick start poll timer if needed.

    // Update our current time before checking IsRetryRequired().
    nudge_tracker_.SetSyncCycleStartTime(TimeTicks::Now());
    if (nudge_tracker_.IsSyncRequired() && CanRunNudgeJobNow()) {
      TrySyncCycleJob();
    }
  }
}

void BraveSyncSchedulerImpl::ScheduleConfiguration(
    const syncer::ConfigurationParams& params) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Only one configuration is allowed at a time. Verify we're not waiting
  // for a pending configure job.
  DCHECK(!pending_configure_params_);

  // Only reconfigure if we have types to download.
  if (!params.types_to_download.Empty()) {
    pending_configure_params_ = std::make_unique<ConfigurationParams>(params);
    TrySyncCycleJob();
  } else {
    SDVLOG(2) << "No change in routing info, calling ready task directly.";
    params.ready_task.Run();
  }
}

void BraveSyncSchedulerImpl::ScheduleClearServerData(
    const syncer::ClearParams& params) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(CLEAR_SERVER_DATA_MODE, mode_);
  DCHECK(!pending_configure_params_);
  DCHECK(!params.report_success_task.is_null());
  DCHECK(started_) << "Scheduler must be running to clear.";

  pending_clear_params_ = std::make_unique<ClearParams>(params);
  TrySyncCycleJob();
}

void BraveSyncSchedulerImpl::ScheduleLocalNudge(
    ModelTypeSet types,
    const base::Location& nudge_location) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void BraveSyncSchedulerImpl::ScheduleLocalRefreshRequest(
    ModelTypeSet types,
    const base::Location& nudge_location) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void BraveSyncSchedulerImpl::ScheduleInvalidationNudge(
    syncer::ModelType model_type,
    std::unique_ptr<syncer::InvalidationInterface> invalidation,
    const base::Location& nudge_location) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void BraveSyncSchedulerImpl::ScheduleInitialSyncNudge(
    syncer::ModelType model_type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void BraveSyncSchedulerImpl::SetNotificationsEnabled(
    bool notifications_enabled) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void BraveSyncSchedulerImpl::Stop() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  SDVLOG(2) << "Stop called";
  // Kill any in-flight method calls.
  weak_ptr_factory_.InvalidateWeakPtrs();
  pending_configure_params_.reset();
  pending_clear_params_.reset();
  poll_timer_.Stop();
  if (started_)
    started_ = false;
}

bool BraveSyncSchedulerImpl::IsAnyThrottleOrBackoff() {
  return false;
}

void BraveSyncSchedulerImpl::OnReceivedShortPollIntervalUpdate(
    const TimeDelta& new_interval) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (new_interval == syncer_short_poll_interval_seconds_)
    return;
  SDVLOG(1) << "Updating short poll interval to " << new_interval.InMinutes()
            << " minutes.";
  syncer_short_poll_interval_seconds_ = new_interval;
  AdjustPolling(UPDATE_INTERVAL);
}

void BraveSyncSchedulerImpl::OnReceivedLongPollIntervalUpdate(
    const TimeDelta& new_interval) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (new_interval == syncer_long_poll_interval_seconds_)
    return;
  SDVLOG(1) << "Updating long poll interval to " << new_interval.InMinutes()
            << " minutes.";
  syncer_long_poll_interval_seconds_ = new_interval;
  AdjustPolling(UPDATE_INTERVAL);
}

const char* BraveSyncSchedulerImpl::GetModeString(
    syncer::SyncScheduler::Mode mode) {
  switch (mode) {
    ENUM_CASE(CONFIGURATION_MODE);
    ENUM_CASE(CLEAR_SERVER_DATA_MODE);
    ENUM_CASE(NORMAL_MODE);
  }
  return "";
}

void BraveSyncSchedulerImpl::DoNudgeSyncCycleJob() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(CanRunNudgeJobNow());

  DVLOG(2) << "Will run normal mode sync cycle with types "
           << ModelTypeSetToString(GetEnabledAndUnblockedTypes());
  SyncCycle cycle(cycle_context_, this);
  bool success = syncer_->NormalSyncShare(GetEnabledAndUnblockedTypes(),
                                          &nudge_tracker_, &cycle);

  if (success) {
    // That cycle took care of any outstanding work we had.
    SDVLOG(2) << "Nudge succeeded.";
    nudge_tracker_.RecordSuccessfulSyncCycle();

    // If this was a canary, we may need to restart the poll timer (the poll
    // timer may have fired while the scheduler was in an error state, ignoring
    // the poll).
    if (!poll_timer_.IsRunning()) {
      SDVLOG(1) << "Canary succeeded, restarting polling.";
      AdjustPolling(UPDATE_INTERVAL);
    }
  }
}

void BraveSyncSchedulerImpl::DoConfigurationSyncCycleJob() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(mode_, CONFIGURATION_MODE);
  DCHECK(pending_configure_params_ != nullptr);

  SDVLOG(2) << "Will run configure SyncShare with types "
            << ModelTypeSetToString(
                   pending_configure_params_->types_to_download);
  SyncCycle cycle(cycle_context_, this);
  bool success =
      syncer_->ConfigureSyncShare(pending_configure_params_->types_to_download,
                                  pending_configure_params_->origin, &cycle);

  if (success) {
    SDVLOG(2) << "Configure succeeded.";
    pending_configure_params_->ready_task.Run();
    pending_configure_params_.reset();
  }
}

void BraveSyncSchedulerImpl::DoClearServerDataSyncCycleJob() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(mode_, CLEAR_SERVER_DATA_MODE);

  SyncCycle cycle(cycle_context_, this);
  const bool success = syncer_->PostClearServerData(&cycle);
  if (!success) {
    return;
  }

  SDVLOG(2) << "Clear succeeded.";
  pending_clear_params_->report_success_task.Run();
  pending_clear_params_.reset();
}

TimeDelta BraveSyncSchedulerImpl::GetPollInterval() {
  return (!cycle_context_->notifications_enabled() ||
          !cycle_context_->ShouldFetchUpdatesBeforeCommit())
             ? syncer_short_poll_interval_seconds_
             : syncer_long_poll_interval_seconds_;
}

void BraveSyncSchedulerImpl::AdjustPolling(PollAdjustType type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!started_)
    return;

  TimeDelta poll_interval = GetPollInterval();
  TimeDelta poll_delay = poll_interval;
  const TimeTicks now = TimeTicks::Now();

  if (type == UPDATE_INTERVAL) {
    if (!last_poll_reset_.is_null()) {
      // Override the delay based on the last successful poll time (if it was
      // set).
      TimeTicks new_poll_time = poll_interval + last_poll_reset_;
      poll_delay = new_poll_time - TimeTicks::Now();

      if (poll_delay < TimeDelta()) {
        // The desired poll time was in the past, so trigger a poll now (the
        // timer will post the task asynchronously, so re-entrancy isn't an
        // issue).
        poll_delay = TimeDelta();
      }
    } else {
      // There was no previous poll. Keep the delay set to the normal interval,
      // as if we had just completed a poll.
      DCHECK_EQ(GetPollInterval(), poll_delay);
      last_poll_reset_ = now;
    }
  } else {
    // Otherwise just restart the timer.
    DCHECK_EQ(FORCE_RESET, type);
    DCHECK_EQ(GetPollInterval(), poll_delay);
    last_poll_reset_ = now;
  }

  SDVLOG(1) << "Updating polling delay to " << poll_delay.InMinutes()
            << " minutes.";

  // Adjust poll rate. Start will reset the timer if it was already running.
  poll_timer_.Start(FROM_HERE, poll_delay, this,
                    &BraveSyncSchedulerImpl::PollTimerCallback);
}

void BraveSyncSchedulerImpl::DoPollSyncCycleJob() {
  SDVLOG(2) << "Polling with types "
            << ModelTypeSetToString(GetEnabledAndUnblockedTypes());
  SyncCycle cycle(cycle_context_, this);
  bool success = syncer_->PollSyncShare(GetEnabledAndUnblockedTypes(), &cycle);

  // Only restart the timer if the poll succeeded. Otherwise rely on normal
  // failure handling to retry with backoff.
  if (success) {
    AdjustPolling(FORCE_RESET);
  } else {
  }
}

bool BraveSyncSchedulerImpl::CanRunNudgeJobNow() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const ModelTypeSet enabled_types = cycle_context_->GetEnabledTypes();
  if (nudge_tracker_.GetBlockedTypes().HasAll(enabled_types)) {
    SDVLOG(1) << "Not running a nudge because we're fully type throttled or "
                 "backed off.";
    return false;
  }

  if (mode_ != NORMAL_MODE) {
    SDVLOG(1) << "Not running nudge because we're not in normal mode.";
    return false;
  }

  return true;
}

void BraveSyncSchedulerImpl::TrySyncCycleJob() {
  // Post call to TrySyncCycleJobImpl on current sequence. Later request for
  // access token will be here.
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&BraveSyncSchedulerImpl::TrySyncCycleJobImpl,
                                weak_ptr_factory_.GetWeakPtr()));
}

void BraveSyncSchedulerImpl::TrySyncCycleJobImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  TimeTicks now = TimeTicks::Now();
  nudge_tracker_.SetSyncCycleStartTime(now);

  if (mode_ == CONFIGURATION_MODE) {
    if (pending_configure_params_) {
      SDVLOG(2) << "Found pending configure job";
      DoConfigurationSyncCycleJob();
    }
  } else if (mode_ == CLEAR_SERVER_DATA_MODE) {
    if (pending_clear_params_) {
      DoClearServerDataSyncCycleJob();
    }
  } else if (CanRunNudgeJobNow()) {
    if (nudge_tracker_.IsSyncRequired()) {
      SDVLOG(2) << "Found pending nudge job";
      DoNudgeSyncCycleJob();
    } else if (((TimeTicks::Now() - last_poll_reset_) >= GetPollInterval())) {
      SDVLOG(2) << "Found pending poll";
      DoPollSyncCycleJob();
    }
  } else {
    // We must be in an error state. Transitioning out of each of these
    // error states should trigger a canary job.
    // DCHECK(IsGlobalThrottle() || IsGlobalBackoff() ||
    //        cycle_context_->connection_manager()->HasInvalidAuthToken());
  }

  // RestartWaiting();
}

void BraveSyncSchedulerImpl::PollTimerCallback() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!syncer_->IsSyncing());

  TrySyncCycleJob();
}

// static
base::Time BraveSyncSchedulerImpl::ComputeLastPollOnStart(
    base::Time last_poll,
    base::TimeDelta poll_interval,
    base::Time now) {
  if (base::FeatureList::IsEnabled(switches::kSyncResetPollIntervalOnStart)) {
    return now;
  }
  // Handle immediate polls on start-up separately.
  if (last_poll + poll_interval <= now) {
    // Doing polls on start-up is generally a risk as other bugs in Chrome
    // might cause start-ups -- potentially synchronized to a specific time.
    // (think about a system timer waking up Chrome).
    // To minimize that risk, we randomly delay polls on start-up to a max
    // of 1% of the poll interval. Assuming a poll rate of 4h, that's at
    // most 2.4 mins.
    base::TimeDelta random_delay = base::RandDouble() * 0.01 * poll_interval;
    return now - (poll_interval - random_delay);
  }
  return last_poll;
}

ModelTypeSet BraveSyncSchedulerImpl::GetEnabledAndUnblockedTypes() {
  ModelTypeSet enabled_types = cycle_context_->GetEnabledTypes();
  ModelTypeSet enabled_protocol_types =
      Intersection(ProtocolTypes(), enabled_types);
  ModelTypeSet blocked_types = nudge_tracker_.GetBlockedTypes();
  return Difference(enabled_protocol_types, blocked_types);
}

void BraveSyncSchedulerImpl::SendInitialSnapshot() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  SyncCycleEvent event(SyncCycleEvent::STATUS_CHANGED);
  event.snapshot = SyncCycle(cycle_context_, this).TakeSnapshot();
  for (auto& observer : *cycle_context_->listeners())
    observer.OnSyncCycleEvent(event);
}

#undef SDVLOG_LOC
#undef SDVLOG
#undef SLOG
#undef ENUM_CASE

}  // namespace brave_sync
