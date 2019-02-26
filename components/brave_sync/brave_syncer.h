/* Copyright (c) 2019 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef BRAVE_COMPONENTS_BRAVE_SYNC_BRAVE_SYNCER_H_
#define BRAVE_COMPONENTS_BRAVE_SYNC_BRAVE_SYNCER_H_

#include <stdint.h>

#include <vector>

#include "base/macros.h"
#include "brave/components/brave_sync/client/brave_sync_client.h"
#include "components/sync/base/model_type.h"
#include "components/sync/base/syncer_error.h"
#include "components/sync/engine_impl/syncer.h"
#include "components/sync/protocol/sync.pb.h"

namespace extensions {
class BraveSyncEventRouter;
}

namespace syncer {

class CancelationSignal;
class CommitProcessor;
class GetUpdatesDelegate;
class NudgeTracker;
class SyncCycle;

}  // namespace syncer

namespace brave_sync {
// A Syncer provides a control interface for driving the sync cycle.  These
// cycles consist of downloading updates, parsing the response (aka. process
// updates), applying updates while resolving conflicts, and committing local
// changes.  Some of these steps may be skipped if they're deemed to be
// unnecessary.
//
// A Syncer instance expects to run on a dedicated thread.  Calls to SyncShare()
// may take an unbounded amount of time because it may block on network I/O, on
// lock contention, or on tasks posted to other threads.
class BraveSyncer : public syncer::Syncer,
                    public SyncMessageHandler {
 public:
  BraveSyncer(
      syncer::CancelationSignal* cancelation_signal,
      extensions::BraveSyncEventRouter* brave_sync_event_router);
  ~BraveSyncer() override;

  // Whether the syncer is in the middle of a sync cycle.
  bool IsSyncing() const;

  // Fetches and applies updates, resolves conflicts and commits local changes
  // for |request_types| as necessary until client and server states are in
  // sync.  The |nudge_tracker| contains state that describes why the client is
  // out of sync and what must be done to bring it back into sync.
  // Returns: false if an error occurred and retries should backoff, true
  // otherwise.
  bool NormalSyncShare(syncer::ModelTypeSet request_types,
                       syncer::NudgeTracker* nudge_tracker,
                       syncer::SyncCycle* cycle) override;

  // Performs an initial download for the |request_types|.  It is assumed that
  // the specified types have no local state, and that their associated change
  // processors are in "passive" mode, so none of the downloaded updates will be
  // applied to the model.  The |source| is sent up to the server for debug
  // purposes.  It describes the reson for performing this initial download.
  // Returns: false if an error occurred and retries should backoff, true
  // otherwise.
  bool ConfigureSyncShare(const syncer::ModelTypeSet& request_types,
                          sync_pb::SyncEnums::GetUpdatesOrigin origin,
                          syncer::SyncCycle* cycle) override;

  // Requests to download updates for the |request_types|.  For a well-behaved
  // client with a working connection to the invalidations server, this should
  // be unnecessary.  It may be invoked periodically to try to keep the client
  // in sync despite bugs or transient failures.
  // Returns: false if an error occurred and retries should backoff, true
  // otherwise.
  bool PollSyncShare(syncer::ModelTypeSet request_types, syncer::SyncCycle* cycle) override;

  // Posts a ClearServerData command.
  // Returns: false if an error occurred and retries should backoff, true
  // otherwise.
  bool PostClearServerData(syncer::SyncCycle* cycle) override;

  // SyncMessageHandler overrides
  void BackgroundSyncStarted(bool startup) override;
  void BackgroundSyncStopped(bool shutdown) override;
  void OnSyncDebug(const std::string& message) override;
  void OnSyncSetupError(const std::string& error) override;
  void OnGetInitData(const std::string& sync_version) override;
  void OnSaveInitData(const Uint8Array& seed,
                      const Uint8Array& device_id) override;
  void OnSyncReady() override;
  void OnGetExistingObjects(const std::string& category_name,
    std::unique_ptr<RecordsList> records,
    const base::Time &last_record_time_stamp,
    const bool is_truncated) override;
  void OnResolvedSyncRecords(const std::string& category_name,
    std::unique_ptr<RecordsList> records) override;
  void OnDeletedSyncUser() override;
  void OnDeleteSyncSiteSettings() override;
  void OnSaveBookmarksBaseOrder(const std::string& order) override;
  void OnSyncWordsPrepared(const std::string& words) override;

 private:
  // Whether an early exist was requested due to a cancelation signal.
  bool ExitRequested();

  bool HandleCycleEnd(syncer::SyncCycle* cycle,
                      sync_pb::SyncEnums::GetUpdatesOrigin origin);

  syncer::CancelationSignal* const cancelation_signal_;

  // Whether the syncer is in the middle of a sync attempt.
  bool is_syncing_;

  extensions::BraveSyncEventRouter* brave_sync_event_router_;

  bool initialized_;

  std::string sync_words_;
  std::string seed_str_;
  std::string device_id_str_;
  std::string bookmarks_base_order_;

  DISALLOW_COPY_AND_ASSIGN(BraveSyncer);
};

}  // namespace brave_sync

#endif  // BRAVE_COMPONENTS_SYNC_BRAVE_SYNCER_H_
