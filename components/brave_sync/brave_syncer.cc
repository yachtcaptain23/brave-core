/* Copyright (c) 2019 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "brave/components/brave_sync/brave_syncer.h"

#include <memory>

#include "base/auto_reset.h"
#include "base/logging.h"
#include "base/trace_event/trace_event.h"
#include "brave/browser/extensions/api/brave_sync_event_router.h"
#include "brave/common/extensions/api/brave_sync.h"
#include "brave/components/brave_sync/client/client_ext_impl_data.h"
#include "brave/components/brave_sync/jslib_const.h"
#include "brave/components/brave_sync/values_conv.h"
#include "components/sync/base/cancelation_signal.h"
#include "components/sync/engine_impl/apply_control_data_updates.h"
#include "components/sync/engine_impl/clear_server_data.h"
#include "components/sync/engine_impl/commit.h"
#include "components/sync/engine_impl/commit_processor.h"
#include "components/sync/engine_impl/cycle/nudge_tracker.h"
#include "components/sync/engine_impl/cycle/sync_cycle.h"
#include "components/sync/engine_impl/get_updates_delegate.h"
#include "components/sync/engine_impl/get_updates_processor.h"
#include "components/sync/engine_impl/net/server_connection_manager.h"
#include "components/sync/syncable/directory.h"

namespace brave_sync {

using syncer::CancelationSignal;
using syncer::ClearServerData;
using syncer::CommitProcessor;
using syncer::SyncCycle;
using syncer::SyncCycleEvent;
using syncer::SyncerError;
using syncer::ModelTypeSet;
using syncer::NudgeTracker;

namespace {

void HandleCycleBegin(SyncCycle* cycle) {
  cycle->mutable_status_controller()->UpdateStartTime();
  cycle->SendEventNotification(SyncCycleEvent::SYNC_CYCLE_BEGIN);
}

}  // namespace

BraveSyncer::BraveSyncer(
      CancelationSignal* cancelation_signal,
      extensions::BraveSyncEventRouter* brave_sync_event_router)
    : syncer::Syncer(cancelation_signal),
    cancelation_signal_(cancelation_signal), is_syncing_(false),
    brave_sync_event_router_(brave_sync_event_router), initialized_(false) {}

BraveSyncer::~BraveSyncer() {}

bool BraveSyncer::IsSyncing() const {
  return is_syncing_;
}

bool BraveSyncer::NormalSyncShare(ModelTypeSet request_types,
                             NudgeTracker* nudge_tracker,
                             SyncCycle* cycle) {
  base::AutoReset<bool> is_syncing(&is_syncing_, true);
  HandleCycleBegin(cycle);
  if (nudge_tracker->IsGetUpdatesRequired() ||
      cycle->context()->ShouldFetchUpdatesBeforeCommit()) {
    VLOG(1) << "Downloading types " << ModelTypeSetToString(request_types);
#if 0
    if (!DownloadAndApplyUpdates(&request_types, cycle,
                                 NormalGetUpdatesDelegate(*nudge_tracker))) {
      return HandleCycleEnd(cycle, nudge_tracker->GetOrigin());
    }
#endif
  }

#if 0
  CommitProcessor commit_processor(
      cycle->context()->model_type_registry()->commit_contributor_map());
  SyncerError commit_result = BuildAndPostCommits(request_types, nudge_tracker,
                                                  cycle, &commit_processor);
  cycle->mutable_status_controller()->set_commit_result(commit_result);
#endif

  return HandleCycleEnd(cycle, nudge_tracker->GetOrigin());
}

bool BraveSyncer::ConfigureSyncShare(const ModelTypeSet& request_types,
                                sync_pb::SyncEnums::GetUpdatesOrigin origin,
                                SyncCycle* cycle) {
  base::AutoReset<bool> is_syncing(&is_syncing_, true);

  // It is possible during configuration that datatypes get unregistered from
  // ModelTypeRegistry before scheduled configure sync cycle gets executed.
  // This happens either because DataTypeController::LoadModels fail and type
  // need to be stopped or during shutdown when all datatypes are stopped. When
  // it happens we should adjust set of types to download to only include
  // registered types.
  ModelTypeSet still_enabled_types =
      Intersection(request_types, cycle->context()->GetEnabledTypes());
  VLOG(1) << "Configuring types " << ModelTypeSetToString(still_enabled_types);
  HandleCycleBegin(cycle);
  // DownloadAndApplyUpdates(&still_enabled_types, cycle,
  //                         ConfigureGetUpdatesDelegate(origin));
  brave_sync_event_router_->LoadClient();
  while (!initialized_);
  return HandleCycleEnd(cycle, origin);
}

bool BraveSyncer::PollSyncShare(ModelTypeSet request_types, SyncCycle* cycle) {
  base::AutoReset<bool> is_syncing(&is_syncing_, true);
  VLOG(1) << "Polling types " << ModelTypeSetToString(request_types);
  HandleCycleBegin(cycle);
  // DownloadAndApplyUpdates(&request_types, cycle, PollGetUpdatesDelegate());

  std::vector<std::string> category_names;
  category_names.push_back(jslib_const::kBookmarks);
  brave_sync_event_router_->FetchSyncRecords(
    category_names, cycle->mutable_status_controller()->sync_start_time(),
    1000);
  return HandleCycleEnd(cycle, sync_pb::SyncEnums::PERIODIC);
}

bool BraveSyncer::PostClearServerData(SyncCycle* cycle) {
  DCHECK(cycle);
  ClearServerData clear_server_data(cycle->context()->account_name());
  return clear_server_data.SendRequest(cycle).value() == SyncerError::SYNCER_OK;
}

void BraveSyncer::BackgroundSyncStarted(bool startup) {
  LOG(ERROR) << __func__;
}

void BraveSyncer::BackgroundSyncStopped(bool shutdown) {
  LOG(ERROR) << __func__;
}

void BraveSyncer::OnSyncDebug(const std::string& message) {
  LOG(ERROR) << __func__ << message;
}

void BraveSyncer::OnSyncSetupError(const std::string& error) {
  LOG(ERROR) << __func__ << error;
}

void BraveSyncer::OnGetInitData(const std::string& sync_version) {
  LOG(ERROR) << __func__;
  Uint8Array seed;
  Uint8Array device_id;

  brave_sync::client_data::Config config;
  config.api_version = "0";
  config.server_url = "https://sync.brave.com";
  config.debug = true;
  extensions::api::brave_sync::Config config_extension;
  ConvertConfig(config, config_extension);
  brave_sync_event_router_->GotInitData(seed, device_id, config_extension, sync_words_);
}

void BraveSyncer::OnSaveInitData(const Uint8Array& seed,
                                          const Uint8Array& device_id) {
  LOG(ERROR) << __func__;
  // TODO(darkdh): need presistent storage, POC always uses new chain
  seed_str_ = StrFromUint8Array(seed);
  device_id_str_ = StrFromUint8Array(device_id);
}

void BraveSyncer::OnSyncReady() {
  if (bookmarks_base_order_.empty()) {
    std::string platform = "POC";
    brave_sync_event_router_->SendGetBookmarksBaseOrder(device_id_str_,
                                                        platform);
    // OnSyncReady will be called by OnSaveBookmarksBaseOrder
    return;
  }

  DCHECK(false == initialized_);
  initialized_ = true;
}

void BraveSyncer::OnGetExistingObjects(
    const std::string& category_name,
    std::unique_ptr<RecordsList> records,
    const base::Time &last_record_time_stamp,
    const bool is_truncated) {

#if 0
  if (category_name == jslib_const::kBookmarks) {
    auto records_and_existing_objects =
        std::make_unique<SyncRecordAndExistingList>();
    bookmark_change_processor_->GetAllSyncData(
        *records.get(), records_and_existing_objects.get());
    brave_sync_event_router_->ResolveSyncRecords(
        category_name, std::move(records_and_existing_objects));
  } else if (category_name == brave_sync::jslib_const::kPreferences) {
    auto existing_records = PrepareResolvedPreferences(*records.get());
    brave_sync_event_router_->ResolveSyncRecords(
        category_name, std::move(existing_records));
  }
#endif
}

void BraveSyncer::OnResolvedSyncRecords(
    const std::string& category_name,
    std::unique_ptr<RecordsList> records) {
#if 0
  if (category_name == brave_sync::jslib_const::kPreferences) {
    OnResolvedPreferences(*records.get());
  } else if (category_name == brave_sync::jslib_const::kBookmarks) {
    bookmark_change_processor_->ApplyChangesFromSyncModel(*records.get());
    bookmark_change_processor_->SendUnsynced(unsynced_send_interval_);
  } else if (category_name == brave_sync::jslib_const::kHistorySites) {
    NOTIMPLEMENTED();
  }
#endif
}

void BraveSyncer::OnDeletedSyncUser() {
  NOTIMPLEMENTED();
}

void BraveSyncer::OnDeleteSyncSiteSettings()  {
  NOTIMPLEMENTED();
}

void BraveSyncer::OnSaveBookmarksBaseOrder(const std::string& order)  {
  DCHECK(!order.empty());
  bookmarks_base_order_ = order;
  OnSyncReady();
}

void BraveSyncer::OnSyncWordsPrepared(const std::string& words) {
  LOG(ERROR) << words;
}

#if 0
bool BraveSyncer::DownloadAndApplyUpdates(ModelTypeSet* request_types,
                                     SyncCycle* cycle,
                                     const GetUpdatesDelegate& delegate) {
  // CommitOnlyTypes() should not be included in the GetUpdates, but should be
  // included in the Commit. We are given a set of types for our SyncShare,
  // and we must do this filtering. Note that |request_types| is also an out
  // param, see below where we update it.
  ModelTypeSet requested_commit_only_types =
      Intersection(*request_types, CommitOnlyTypes());
  ModelTypeSet download_types =
      Difference(*request_types, requested_commit_only_types);
  GetUpdatesProcessor get_updates_processor(
      cycle->context()->model_type_registry()->update_handler_map(), delegate);
  SyncerError download_result;
  do {
    download_result =
        get_updates_processor.DownloadUpdates(&download_types, cycle);
  } while (download_result.value() == SyncerError::SERVER_MORE_TO_DOWNLOAD);

  // It is our responsibility to propagate the removal of types that occurred in
  // GetUpdatesProcessor::DownloadUpdates().
  *request_types = Union(download_types, requested_commit_only_types);

  // Exit without applying if we're shutting down or an error was detected.
  if (download_result.value() != SyncerError::SYNCER_OK || ExitRequested())
    return false;

  {
    TRACE_EVENT0("sync", "ApplyUpdates");

    // Control type updates always get applied first.
    ApplyControlDataUpdates(cycle->context()->directory());

    // Apply updates to the other types. May or may not involve cross-thread
    // traffic, depending on the underlying update handlers and the GU type's
    // delegate.
    get_updates_processor.ApplyUpdates(download_types,
                                       cycle->mutable_status_controller());

    cycle->context()->set_hierarchy_conflict_detected(
        cycle->status_controller().num_hierarchy_conflicts() > 0);
    cycle->SendEventNotification(SyncCycleEvent::STATUS_CHANGED);
  }

  return !ExitRequested();
}

SyncerError BraveSyncer::BuildAndPostCommits(const ModelTypeSet& request_types,
                                        NudgeTracker* nudge_tracker,
                                        SyncCycle* cycle,
                                        CommitProcessor* commit_processor) {
  VLOG(1) << "Committing from types " << ModelTypeSetToString(request_types);

  // The ExitRequested() check is unnecessary, since we should start getting
  // errors from the ServerConnectionManager if an exist has been requested.
  // However, it doesn't hurt to check it anyway.
  while (!ExitRequested()) {
    std::unique_ptr<Commit> commit(
        Commit::Init(request_types, cycle->context()->GetEnabledTypes(),
                     cycle->context()->max_commit_batch_size(),
                     cycle->context()->account_name(),
                     cycle->context()->directory()->cache_guid(),
                     cycle->context()->cookie_jar_mismatch(),
                     cycle->context()->cookie_jar_empty(), commit_processor,
                     cycle->context()->extensions_activity()));
    if (!commit) {
      break;
    }

    SyncerError error = commit->PostAndProcessResponse(
        nudge_tracker, cycle, cycle->mutable_status_controller(),
        cycle->context()->extensions_activity());
    commit->CleanUp();
    if (error.value() != SyncerError::SYNCER_OK) {
      return error;
    }
  }

  return SyncerError(SyncerError::SYNCER_OK);
}
#endif

bool BraveSyncer::ExitRequested() {
  return cancelation_signal_->IsSignalled();
}

bool BraveSyncer::HandleCycleEnd(SyncCycle* cycle,
                            sync_pb::SyncEnums::GetUpdatesOrigin origin) {
  if (ExitRequested())
    return false;

  bool success =
      !HasSyncerError(cycle->status_controller().model_neutral_state());
  if (success && origin == sync_pb::SyncEnums::PERIODIC) {
    cycle->mutable_status_controller()->UpdatePollTime();
  }
  cycle->SendSyncCycleEndEventNotification(origin);

  return success;
}

}  // namespace brave_sync
