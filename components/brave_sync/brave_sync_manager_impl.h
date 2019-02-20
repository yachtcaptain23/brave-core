/* Copyright (c) 2019 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef BRAVE_COMPONENTS_BRAVE_SYNC_BRAVE_SYNC_MANAGER_IMPL_H_
#define BRAVE_COMPONENTS_BRAVE_SYNC_BRAVE_SYNC_MANAGER_IMPL_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/sequence_checker.h"
#include "brave/components/brave_sync/brave_sync_scheduler_impl.h"
#include "components/sync/base/cryptographer.h"
#include "components/sync/base/time.h"
#include "components/sync/engine/sync_manager.h"
#include "components/sync/engine_impl/all_status.h"
#include "components/sync/engine_impl/debug_info_event_listener.h"
#include "components/sync/engine_impl/events/protocol_event_buffer.h"
#include "components/sync/engine_impl/js_mutation_event_observer.h"
#include "components/sync/engine_impl/js_sync_encryption_handler_observer.h"
#include "components/sync/engine_impl/js_sync_manager_observer.h"
#include "components/sync/engine_impl/net/server_connection_manager.h"
#include "components/sync/engine_impl/nudge_handler.h"
#include "components/sync/engine_impl/sync_encryption_handler_impl.h"
#include "components/sync/engine_impl/sync_engine_event_listener.h"
#include "components/sync/js/js_backend.h"
#include "components/sync/syncable/change_reorder_buffer.h"
#include "components/sync/syncable/directory_change_delegate.h"
#include "components/sync/syncable/user_share.h"
#include "services/network/public/cpp/network_connection_tracker.h"

namespace syncer {

class ModelTypeRegistry;
class SyncCycleContext;
class TypeDebugInfoObserver;

}  // namespace syncer

namespace brave_sync {

using syncer::syncable::ImmutableWriteTransactionInfo;

// SyncManager encapsulates syncable::Directory and serves as the parent of all
// other objects in the sync API.  If multiple threads interact with the same
// local sync repository (i.e. the same sqlite database), they should share a
// single SyncManager instance.  The caller should typically create one
// SyncManager for the lifetime of a user session.
//
// Unless stated otherwise, all methods of SyncManager should be called on the
// same thread.
class BraveSyncManagerImpl
    : public syncer::SyncManager,
      public network::NetworkConnectionTracker::NetworkConnectionObserver,
      public syncer::JsBackend,
      public syncer::syncable::DirectoryChangeDelegate,
      public syncer::SyncEncryptionHandler::Observer,
      public syncer::NudgeHandler {
 public:
  // Create an uninitialized SyncManager.  Callers must Init() before using.
  // |network_connection_tracker| must not be null and must outlive this object.
  BraveSyncManagerImpl(
      const std::string& name,
      network::NetworkConnectionTracker* network_connection_tracker);
  ~BraveSyncManagerImpl() override;

  // SyncManager implementation.
  void Init(InitArgs* args) override;
  syncer::ModelTypeSet InitialSyncEndedTypes() override;
  syncer::ModelTypeSet GetTypesWithEmptyProgressMarkerToken(
      syncer::ModelTypeSet types) override;
  void PurgePartiallySyncedTypes() override;
  void PurgeDisabledTypes(syncer::ModelTypeSet to_purge,
                          syncer::ModelTypeSet to_journal,
                          syncer::ModelTypeSet to_unapply) override;
  void UpdateCredentials(const syncer::SyncCredentials& credentials) override;
  void InvalidateCredentials() override;
  void StartSyncingNormally(base::Time last_poll_time) override;
  void StartConfiguration() override;
  void ConfigureSyncer(syncer::ConfigureReason reason,
                       syncer::ModelTypeSet to_download,
                       SyncFeatureState sync_feature_state,
                       const base::Closure& ready_task) override;
  void SetInvalidatorEnabled(bool invalidator_enabled) override;
  void OnIncomingInvalidation(
      syncer::ModelType type,
      std::unique_ptr<syncer::InvalidationInterface> invalidation) override;
  void AddObserver(SyncManager::Observer* observer) override;
  void RemoveObserver(SyncManager::Observer* observer) override;
  syncer::SyncStatus GetDetailedStatus() const override;
  void SaveChanges() override;
  void ShutdownOnSyncThread() override;
  syncer::UserShare* GetUserShare() override;
  syncer::ModelTypeConnector* GetModelTypeConnector() override;
  std::unique_ptr<syncer::ModelTypeConnector>
    GetModelTypeConnectorProxy() override;
  const std::string cache_guid() override;
  bool ReceivedExperiment(syncer::Experiments* experiments) override;
  bool HasUnsyncedItemsForTest() override;
  syncer::SyncEncryptionHandler* GetEncryptionHandler() override;
  std::vector<std::unique_ptr<syncer::ProtocolEvent>>
    GetBufferedProtocolEvents() override;
  void RegisterDirectoryTypeDebugInfoObserver(
      syncer::TypeDebugInfoObserver* observer) override;
  void UnregisterDirectoryTypeDebugInfoObserver(
      syncer::TypeDebugInfoObserver* observer) override;
  bool HasDirectoryTypeDebugInfoObserver(
      syncer::TypeDebugInfoObserver* observer) override;
  void RequestEmitDebugInfo() override;
  void ClearServerData(const base::Closure& callback) override;
  void OnCookieJarChanged(bool account_mismatch, bool empty_jar) override;
  void OnMemoryDump(base::trace_event::ProcessMemoryDump* pmd) override;
  void UpdateInvalidationClientId(const std::string& client_id) override;

  // SyncEncryptionHandler::Observer implementation.
  void OnPassphraseRequired(
      syncer::PassphraseRequiredReason reason,
      const syncer::KeyDerivationParams& key_derivation_params,
      const sync_pb::EncryptedData& pending_keys) override;
  void OnPassphraseAccepted() override;
  void OnBootstrapTokenUpdated(const std::string& bootstrap_token,
                               syncer::BootstrapTokenType type) override;
  void OnEncryptedTypesChanged(syncer::ModelTypeSet encrypted_types,
                               bool encrypt_everything) override;
  void OnEncryptionComplete() override;
  void OnCryptographerStateChanged(
      syncer::Cryptographer* cryptographer) override;
  void OnPassphraseTypeChanged(syncer::PassphraseType type,
                               base::Time explicit_passphrase_time) override;
  void OnLocalSetPassphraseEncryption(
      const syncer::SyncEncryptionHandler::NigoriState& nigori_state) override;

  // JsBackend implementation.
  void SetJsEventHandler(
      const syncer::WeakHandle<syncer::JsEventHandler>& event_handler) override;

  // DirectoryChangeDelegate implementation.
  // This listener is called upon completion of a syncable transaction, and
  // builds the list of sync-engine initiated changes that will be forwarded to
  // the SyncManager's Observers.
  void HandleTransactionCompleteChangeEvent(
      syncer::ModelTypeSet models_with_changes) override;
  syncer::ModelTypeSet HandleTransactionEndingChangeEvent(
      const ImmutableWriteTransactionInfo& write_transaction_info,
      syncer::syncable::BaseTransaction* trans) override;
  void HandleCalculateChangesChangeEventFromSyncApi(
      const ImmutableWriteTransactionInfo& write_transaction_info,
      syncer::syncable::BaseTransaction* trans,
      std::vector<int64_t>* entries_changed) override;
  void HandleCalculateChangesChangeEventFromSyncer(
      const ImmutableWriteTransactionInfo& write_transaction_info,
      syncer::syncable::BaseTransaction* trans,
      std::vector<int64_t>* entries_changed) override;

  // Handle explicit requests to fetch updates for the given types.
  void RefreshTypes(syncer::ModelTypeSet types) override;

  // NetworkConnectionTracker::NetworkConnectionObserver implementation.
  void OnConnectionChanged(network::mojom::ConnectionType type) override;

  // NudgeHandler implementation.
  void NudgeForInitialDownload(syncer::ModelType type) override;
  void NudgeForCommit(syncer::ModelType type) override;
  void NudgeForRefresh(syncer::ModelType type) override;

  const syncer::SyncScheduler* scheduler() const;

 protected:
  // Helper functions.  Virtual for testing.
  virtual void NotifyInitializationSuccess();
  virtual void NotifyInitializationFailure();

 private:
  syncer::syncable::Directory* directory();

  // Open the directory named with |username|.
  bool OpenDirectory(const std::string& username);

  void RequestNudgeForDataTypes(const base::Location& nudge_location,
                                syncer::ModelTypeSet type);

  base::FilePath database_path_;

  const std::string name_;

  network::NetworkConnectionTracker* network_connection_tracker_;

  SEQUENCE_CHECKER(sequence_checker_);

  // We give a handle to share_ to clients of the API for use when constructing
  // any transaction type.
  syncer::UserShare share_;

  // This can be called from any thread, but only between calls to
  // OpenDirectory() and ShutdownOnSyncThread().
  syncer::WeakHandle<SyncManager::ChangeObserver> change_observer_;

  base::ObserverList<SyncManager::Observer>::Unchecked observers_;

  // Maintains state that affects the way we interact with different sync types.
  // This state changes when entering or exiting a configuration cycle.
  std::unique_ptr<syncer::ModelTypeRegistry> model_type_registry_;

  // The scheduler that runs the Syncer. Needs to be explicitly
  // Start()ed.
  std::unique_ptr<syncer::SyncScheduler> scheduler_;

  // A multi-purpose status watch object that aggregates stats from various
  // sync components.
  syncer::AllStatus allstatus_;

  // Each element of this map is a store of change records produced by
  // HandleChangeEventFromSyncer during the CALCULATE_CHANGES step. The changes
  // are grouped by model type, and are stored here in tree order to be
  // forwarded to the observer slightly later, at the TRANSACTION_ENDING step
  // by HandleTransactionEndingChangeEvent. The list is cleared after observer
  // finishes processing.
  using ChangeRecordMap = std::map<int, syncer::ImmutableChangeRecordList>;
  ChangeRecordMap change_records_;

  SyncManager::ChangeDelegate* change_delegate_;

  // Set to true once Init has been called.
  bool initialized_;

  bool observing_network_connectivity_changes_;

  // These are for interacting with chrome://sync-internals.
  syncer::JsSyncManagerObserver js_sync_manager_observer_;
  syncer::JsMutationEventObserver js_mutation_event_observer_;
  syncer::JsSyncEncryptionHandlerObserver js_sync_encryption_handler_observer_;

  // This is for keeping track of client events to send to the server.
  syncer::DebugInfoEventListener debug_info_event_listener_;

  syncer::ProtocolEventBuffer protocol_event_buffer_;

  base::Closure report_unrecoverable_error_function_;

  // Sync's encryption handler. It tracks the set of encrypted types, manages
  // changing passphrases, and in general handles sync-specific interactions
  // with the cryptographer.
  std::unique_ptr<syncer::SyncEncryptionHandlerImpl> sync_encryption_handler_;

  base::WeakPtrFactory<BraveSyncManagerImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(BraveSyncManagerImpl);
};

}  // namespace brave_sync

#endif  // BRAVE_COMPONENTS_BRAVE_SYNC_BRAVE_SYNC_MANAGER_IMPL_H_
