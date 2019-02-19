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
#include "components/sync/protocol/proto_enum_conversions.h"
#include "components/sync/protocol/sync.pb.h"

namespace brave_sync {

// Helper macros to log with the syncer thread name; useful when there
// are multiple syncer threads involved.

#define SLOG(severity) LOG(severity) << name_ << ": "

#define SDVLOG(verbose_level) DVLOG(verbose_level) << name_ << ": "

#define SDVLOG_LOC(from_here, verbose_level) \
  DVLOG_LOC(from_here, verbose_level) << name_ << ": "

BraveSyncSchedulerImpl::BraveSyncSchedulerImpl(const std::string& name)
    : name_(name),
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
}

void BraveSyncSchedulerImpl::ScheduleConfiguration(
    const syncer::ConfigurationParams& params) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void BraveSyncSchedulerImpl::ScheduleClearServerData(
    const syncer::ClearParams& params) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void BraveSyncSchedulerImpl::ScheduleLocalNudge(
    syncer::ModelTypeSet types,
    const base::Location& nudge_location) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void BraveSyncSchedulerImpl::ScheduleLocalRefreshRequest(
    syncer::ModelTypeSet types,
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
  if (started_)
    started_ = false;
}

bool BraveSyncSchedulerImpl::IsAnyThrottleOrBackoff() {
  return false;
}

#undef SDVLOG_LOC
#undef SDVLOG
#undef SLOG
#undef ENUM_CASE

}  // namespace brave_sync
