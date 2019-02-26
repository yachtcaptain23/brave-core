/* Copyright (c) 2019 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "brave/components/brave_sync/brave_sync_manager_impl.h"

#include <stddef.h>

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/metrics/histogram_macros.h"
#include "base/observer_list.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/values.h"
#include "brave/components/brave_sync/brave_syncer.h"
#include "components/sync/base/cancelation_signal.h"
#include "components/sync/base/experiments.h"
#include "components/sync/base/invalidation_interface.h"
#include "components/sync/base/model_type.h"
#include "components/sync/base/nigori.h"
#include "components/sync/engine/configure_reason.h"
#include "components/sync/engine/engine_components_factory.h"
#include "components/sync/engine/engine_util.h"
#include "components/sync/engine/net/http_post_provider_factory.h"
#include "components/sync/engine/polling_constants.h"
#include "components/sync/engine_impl/cycle/directory_type_debug_info_emitter.h"
#include "components/sync/engine_impl/model_type_connector_proxy.h"
#include "components/sync/engine_impl/sync_scheduler.h"
#include "components/sync/engine_impl/syncer.h"
#include "components/sync/engine_impl/syncer_types.h"
#include "components/sync/engine_impl/uss_migrator.h"
#include "components/sync/protocol/sync.pb.h"
#include "components/sync/syncable/base_node.h"
#include "components/sync/syncable/directory.h"
#include "components/sync/syncable/directory_backing_store.h"
#include "components/sync/syncable/entry.h"
#include "components/sync/syncable/read_node.h"
#include "components/sync/syncable/read_transaction.h"
#include "components/sync/syncable/write_node.h"
#include "components/sync/syncable/write_transaction.h"

namespace syncer {

namespace {

sync_pb::SyncEnums::GetUpdatesOrigin GetOriginFromReason(
    ConfigureReason reason) {
  switch (reason) {
    case CONFIGURE_REASON_RECONFIGURATION:
      return sync_pb::SyncEnums::RECONFIGURATION;
    case CONFIGURE_REASON_MIGRATION:
      return sync_pb::SyncEnums::MIGRATION;
    case CONFIGURE_REASON_NEW_CLIENT:
      return sync_pb::SyncEnums::NEW_CLIENT;
    case CONFIGURE_REASON_NEWLY_ENABLED_DATA_TYPE:
    case CONFIGURE_REASON_CRYPTO:
    case CONFIGURE_REASON_CATCH_UP:
      return sync_pb::SyncEnums::NEWLY_SUPPORTED_DATATYPE;
    case CONFIGURE_REASON_PROGRAMMATIC:
      return sync_pb::SyncEnums::PROGRAMMATIC;
    case CONFIGURE_REASON_UNKNOWN:
      NOTREACHED();
  }
  return sync_pb::SyncEnums::UNKNOWN_ORIGIN;
}

}  // namespace

}   // namespace syncer

namespace brave_sync {

using syncer::syncable::SPECIFICS;
using syncer::syncable::UNIQUE_POSITION;
using syncer::syncable::ImmutableEntryKernelMutationMap;
using syncer::BootstrapTokenType;
using syncer::ConfigurationParams;
using syncer::ConfigureReason;
using syncer::Cryptographer;
using syncer::Experiments;
using syncer::FIRST_REAL_MODEL_TYPE;
using syncer::GetModelTypeFromSpecifics;
using syncer::HttpResponse;
using syncer::KeyDerivationParams;
using syncer::MODEL_TYPE_COUNT;
using syncer::ModelType;
using syncer::ModelTypeConnector;
using syncer::ModelTypeSet;
using syncer::PassphraseRequiredReason;
using syncer::PassphraseType;
using syncer::ReadTransaction;
using syncer::SyncCredentials;
using syncer::SyncEncryptionHandler;
using syncer::SyncEncryptionHandlerImpl;
using syncer::SyncScheduler;
using syncer::SyncStatus;
using syncer::TypeDebugInfoObserver;
using syncer::UserShare;
using syncer::WeakHandle;

BraveSyncManagerImpl::BraveSyncManagerImpl(
    const std::string& name,
    network::NetworkConnectionTracker* network_connection_tracker)
    : name_(name),
      network_connection_tracker_(network_connection_tracker),
      change_delegate_(nullptr),
      initialized_(false),
      observing_network_connectivity_changes_(false),
      weak_ptr_factory_(this) {
}

BraveSyncManagerImpl::~BraveSyncManagerImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!initialized_);
}

ModelTypeSet BraveSyncManagerImpl::InitialSyncEndedTypes() {
  DCHECK(initialized_);
  return model_type_registry_->GetInitialSyncEndedTypes();
}

ModelTypeSet BraveSyncManagerImpl::GetTypesWithEmptyProgressMarkerToken(
    ModelTypeSet types) {
  ModelTypeSet result;
  for (ModelType type : types) {
    sync_pb::DataTypeProgressMarker marker;
    directory()->GetDownloadProgress(type, &marker);

    if (marker.token().empty())
      result.Put(type);
  }
  return result;
}

void BraveSyncManagerImpl::ConfigureSyncer(ConfigureReason reason,
                                      ModelTypeSet to_download,
                                      SyncFeatureState sync_feature_state,
                                      const base::Closure& ready_task) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!ready_task.is_null());
  DCHECK(initialized_);

  // Don't download non-blocking types that have already completed initial sync.
  to_download.RemoveAll(
      model_type_registry_->GetInitialSyncDoneNonBlockingTypes());

  DVLOG(1) << "Configuring -"
           << "\n\t"
           << "types to download: " << ModelTypeSetToString(to_download);
  ConfigurationParams params(syncer::GetOriginFromReason(reason), to_download,
                             ready_task);

  scheduler_->Start(SyncScheduler::CONFIGURATION_MODE, base::Time());
  scheduler_->ScheduleConfiguration(params);
  if (sync_feature_state != SyncFeatureState::INITIALIZING) {
    cycle_context_->set_is_sync_feature_enabled(sync_feature_state ==
                                                SyncFeatureState::ON);
  }
}

void BraveSyncManagerImpl::Init(InitArgs* args) {
  DCHECK(!initialized_);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(args->post_factory);
  DCHECK(!args->short_poll_interval.is_zero());
  DCHECK(!args->long_poll_interval.is_zero());
  if (!args->enable_local_sync_backend) {
    // DCHECK(!args->credentials.account_id.empty());
  }
  DCHECK(args->cancelation_signal);
  DVLOG(1) << "SyncManager starting Init...";

  change_delegate_ = args->change_delegate;

  AddObserver(&js_sync_manager_observer_);
  SetJsEventHandler(args->event_handler);

  AddObserver(&debug_info_event_listener_);

  database_path_ = args->database_location.Append(
      syncer::syncable::Directory::kSyncDatabaseFilename);
  report_unrecoverable_error_function_ =
      args->report_unrecoverable_error_function;

  allstatus_.SetHasKeystoreKey(
      !args->restored_keystore_key_for_bootstrapping.empty());
  sync_encryption_handler_ = std::make_unique<SyncEncryptionHandlerImpl>(
      &share_, args->encryptor, args->restored_key_for_bootstrapping,
      args->restored_keystore_key_for_bootstrapping,
      base::BindRepeating(&syncer::Nigori::GenerateScryptSalt));
  sync_encryption_handler_->AddObserver(this);
  sync_encryption_handler_->AddObserver(&debug_info_event_listener_);
  sync_encryption_handler_->AddObserver(&js_sync_encryption_handler_observer_);

  base::FilePath absolute_db_path = database_path_;
  DCHECK(absolute_db_path.IsAbsolute());

  std::unique_ptr<syncer::syncable::DirectoryBackingStore> backing_store =
      args->engine_components_factory->BuildDirectoryBackingStore(
          syncer::EngineComponentsFactory::STORAGE_ON_DISK,
          args->credentials.account_id, absolute_db_path);

  DCHECK(backing_store);
  share_.directory = std::make_unique<syncer::syncable::Directory>(
      std::move(backing_store), args->unrecoverable_error_handler,
      report_unrecoverable_error_function_, sync_encryption_handler_.get(),
      sync_encryption_handler_->GetCryptographerUnsafe());
  share_.sync_credentials = args->credentials;

  // UserShare is accessible to a lot of code that doesn't need access to the
  // sync token so clear sync_token from the UserShare.
  share_.sync_credentials.sync_token = "";

  DVLOG(1) << "Username: " << args->credentials.email;
  DVLOG(1) << "AccountId: " << args->credentials.account_id;
  if (!OpenDirectory(args->credentials.account_id)) {
    NotifyInitializationFailure();
    LOG(ERROR) << "Sync manager initialization failed!";
    return;
  }

  // Now that we have opened the Directory we can restore any previously saved
  // nigori specifics.
  if (args->saved_nigori_state) {
    sync_encryption_handler_->RestoreNigori(*args->saved_nigori_state);
    args->saved_nigori_state.reset();
  }

  std::string sync_id = directory()->cache_guid();

  DVLOG(1) << "Setting sync client ID: " << sync_id;
  allstatus_.SetSyncId(sync_id);
  DVLOG(1) << "Setting invalidator client ID: " << args->invalidator_client_id;
  allstatus_.SetInvalidatorClientId(args->invalidator_client_id);

  model_type_registry_ = std::make_unique<syncer::ModelTypeRegistry>(
      args->workers, &share_, this, base::Bind(&syncer::MigrateDirectoryData),
      args->cancelation_signal);
  sync_encryption_handler_->AddObserver(model_type_registry_.get());

  // Build a SyncCycleContext and store the worker in it.
  DVLOG(1) << "Sync is bringing up SyncCycleContext.";
  std::vector<syncer::SyncEngineEventListener*> listeners;
  listeners.push_back(&allstatus_);
  cycle_context_ = args->engine_components_factory->BuildContext(
      nullptr, directory(), args->extensions_activity,
      listeners, &debug_info_event_listener_, model_type_registry_.get(),
      args->invalidator_client_id, args->short_poll_interval,
      args->long_poll_interval);

  scheduler_ = std::make_unique<BraveSyncSchedulerImpl>(name_,
                                                        cycle_context_.get(),
                                                        new BraveSyncer(args->cancelation_signal,
                                                                        args->brave_sync_event_router));

  scheduler_->Start(SyncScheduler::CONFIGURATION_MODE, base::Time());

  initialized_ = true;

  if (!args->enable_local_sync_backend) {
    network_connection_tracker_->AddNetworkConnectionObserver(this);
    observing_network_connectivity_changes_ = true;

    UpdateCredentials(args->credentials);
  } else {
    scheduler_->OnCredentialsUpdated();
  }

  NotifyInitializationSuccess();
}

void BraveSyncManagerImpl::NotifyInitializationSuccess() {
  for (auto& observer : observers_) {
    observer.OnInitializationComplete(
        MakeWeakHandle(weak_ptr_factory_.GetWeakPtr()),
        MakeWeakHandle(debug_info_event_listener_.GetWeakPtr()), true,
        InitialSyncEndedTypes());
  }
}

void BraveSyncManagerImpl::NotifyInitializationFailure() {
  for (auto& observer : observers_) {
    observer.OnInitializationComplete(
        MakeWeakHandle(weak_ptr_factory_.GetWeakPtr()),
        MakeWeakHandle(debug_info_event_listener_.GetWeakPtr()), false,
        ModelTypeSet());
  }
}

void BraveSyncManagerImpl::OnPassphraseRequired(
    PassphraseRequiredReason reason,
    const KeyDerivationParams& key_derivation_params,
    const sync_pb::EncryptedData& pending_keys) {
  // Does nothing.
}

void BraveSyncManagerImpl::OnPassphraseAccepted() {
  // Does nothing.
}

void BraveSyncManagerImpl::OnBootstrapTokenUpdated(
    const std::string& bootstrap_token,
    BootstrapTokenType type) {
  if (type == syncer::KEYSTORE_BOOTSTRAP_TOKEN)
    allstatus_.SetHasKeystoreKey(true);
}

void BraveSyncManagerImpl::OnEncryptedTypesChanged(ModelTypeSet encrypted_types,
                                              bool encrypt_everything) {
  allstatus_.SetEncryptedTypes(encrypted_types);
}

void BraveSyncManagerImpl::OnEncryptionComplete() {
  // Does nothing.
}

void BraveSyncManagerImpl::OnCryptographerStateChanged(
    Cryptographer* cryptographer) {
  allstatus_.SetCryptographerReady(cryptographer->is_ready());
  allstatus_.SetCryptoHasPendingKeys(cryptographer->has_pending_keys());
  allstatus_.SetKeystoreMigrationTime(
      sync_encryption_handler_->migration_time());
}

void BraveSyncManagerImpl::OnPassphraseTypeChanged(
    PassphraseType type,
    base::Time explicit_passphrase_time) {
  allstatus_.SetPassphraseType(type);
  allstatus_.SetKeystoreMigrationTime(
      sync_encryption_handler_->migration_time());
}

void BraveSyncManagerImpl::OnLocalSetPassphraseEncryption(
    const SyncEncryptionHandler::NigoriState& nigori_state) {}

void BraveSyncManagerImpl::StartSyncingNormally(
    base::Time last_poll_time) {
  // Start the sync scheduler.
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  scheduler_->Start(SyncScheduler::NORMAL_MODE, last_poll_time);
}

void BraveSyncManagerImpl::StartConfiguration() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  scheduler_->Start(SyncScheduler::CONFIGURATION_MODE, base::Time());
}

syncer::syncable::Directory* BraveSyncManagerImpl::directory() {
  return share_.directory.get();
}

const SyncScheduler* BraveSyncManagerImpl::scheduler() const {
  return scheduler_.get();
}

bool BraveSyncManagerImpl::OpenDirectory(const std::string& username) {
  DCHECK(!initialized_) << "Should only happen once";

  // Set before Open().
  change_observer_ = MakeWeakHandle(js_mutation_event_observer_.AsWeakPtr());
  WeakHandle<syncer::syncable::TransactionObserver> transaction_observer(
      MakeWeakHandle(js_mutation_event_observer_.AsWeakPtr()));

  syncer::syncable::DirOpenResult open_result =
    syncer::syncable::NOT_INITIALIZED;
  open_result = directory()->Open(username, this, transaction_observer);
  if (open_result != syncer::syncable::OPENED) {
    LOG(ERROR) << "Could not open share for:" << username;
    return false;
  }

  // Unapplied datatypes (those that do not have initial sync ended set) get
  // re-downloaded during any configuration. But, it's possible for a datatype
  // to have a progress marker but not have initial sync ended yet, making
  // it a candidate for migration. This is a problem, as the DataTypeManager
  // does not support a migration while it's already in the middle of a
  // configuration. As a result, any partially synced datatype can stall the
  // DTM, waiting for the configuration to complete, which it never will due
  // to the migration error. In addition, a partially synced nigori will
  // trigger the migration logic before the backend is initialized, resulting
  // in crashes. We therefore detect and purge any partially synced types as
  // part of initialization.
  PurgePartiallySyncedTypes();
  return true;
}

void BraveSyncManagerImpl::PurgePartiallySyncedTypes() {
  ModelTypeSet partially_synced_types = ModelTypeSet::All();
  partially_synced_types.RemoveAll(directory()->InitialSyncEndedTypes());
  partially_synced_types.RemoveAll(
      GetTypesWithEmptyProgressMarkerToken(ModelTypeSet::All()));

  DVLOG(1) << "Purging partially synced types "
           << ModelTypeSetToString(partially_synced_types);
  UMA_HISTOGRAM_COUNTS_1M("Sync.PartiallySyncedTypes",
                          partially_synced_types.Size());
  directory()->PurgeEntriesWithTypeIn(partially_synced_types, ModelTypeSet(),
                                      ModelTypeSet());
}

void BraveSyncManagerImpl::PurgeDisabledTypes(ModelTypeSet to_purge,
                                         ModelTypeSet to_journal,
                                         ModelTypeSet to_unapply) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(initialized_);
  DVLOG(1) << "Purging disabled types:\n\t"
           << "types to purge: " << ModelTypeSetToString(to_purge) << "\n\t"
           << "types to journal: " << ModelTypeSetToString(to_journal) << "\n\t"
           << "types to unapply: " << ModelTypeSetToString(to_unapply);
  DCHECK(to_purge.HasAll(to_journal));
  DCHECK(to_purge.HasAll(to_unapply));
  directory()->PurgeEntriesWithTypeIn(to_purge, to_journal, to_unapply);
}

void BraveSyncManagerImpl::UpdateCredentials(
    const SyncCredentials& credentials) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(initialized_);
  // DCHECK(!credentials.account_id.empty());

  observing_network_connectivity_changes_ = true;

  scheduler_->OnCredentialsUpdated();

  // TODO(zea): pass the credential age to the debug info event listener.
}

void BraveSyncManagerImpl::InvalidateCredentials() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void BraveSyncManagerImpl::AddObserver(SyncManager::Observer* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observers_.AddObserver(observer);
}

void BraveSyncManagerImpl::RemoveObserver(SyncManager::Observer* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observers_.RemoveObserver(observer);
}

void BraveSyncManagerImpl::ShutdownOnSyncThread() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Prevent any in-flight method calls from running.  Also
  // invalidates |weak_handle_this_| and |change_observer_|.
  weak_ptr_factory_.InvalidateWeakPtrs();
  js_mutation_event_observer_.InvalidateWeakPtrs();

  scheduler_.reset();
  cycle_context_.reset();

  if (model_type_registry_)
    sync_encryption_handler_->RemoveObserver(model_type_registry_.get());

  model_type_registry_.reset();

  if (sync_encryption_handler_) {
    sync_encryption_handler_->RemoveObserver(&debug_info_event_listener_);
    sync_encryption_handler_->RemoveObserver(this);
  }

  SetJsEventHandler(WeakHandle<syncer::JsEventHandler>());
  RemoveObserver(&js_sync_manager_observer_);

  RemoveObserver(&debug_info_event_listener_);

  network_connection_tracker_->RemoveNetworkConnectionObserver(this);
  observing_network_connectivity_changes_ = false;

  if (initialized_ && directory()) {
    directory()->SaveChanges();
  }

  share_.directory.reset();

  change_delegate_ = nullptr;

  initialized_ = false;

  // We reset these here, since only now we know they will not be
  // accessed from other threads (since we shut down everything).
  change_observer_.Reset();
}

void BraveSyncManagerImpl::OnConnectionChanged(
    network::mojom::ConnectionType type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!observing_network_connectivity_changes_) {
    DVLOG(1) << "Network change dropped.";
    return;
  }
  DVLOG(1) << "Network change detected.";
  scheduler_->OnConnectionStatusChange(type);
}

void BraveSyncManagerImpl::HandleTransactionCompleteChangeEvent(
    ModelTypeSet models_with_changes) {
  // This notification happens immediately after the transaction mutex is
  // released. This allows work to be performed without blocking other threads
  // from acquiring a transaction.
  if (!change_delegate_)
    return;

  // Call commit.
  for (ModelType type : models_with_changes) {
    change_delegate_->OnChangesComplete(type);
    change_observer_.Call(
        FROM_HERE, &SyncManager::ChangeObserver::OnChangesComplete, type);
  }
}

ModelTypeSet BraveSyncManagerImpl::HandleTransactionEndingChangeEvent(
    const ImmutableWriteTransactionInfo& write_transaction_info,
    syncer::syncable::BaseTransaction* trans) {
  // This notification happens immediately before a syncable WriteTransaction
  // falls out of scope. It happens while the channel mutex is still held,
  // and while the transaction mutex is held, so it cannot be re-entrant.
  if (!change_delegate_ || change_records_.empty())
    return ModelTypeSet();

  // This will continue the WriteTransaction using a read only wrapper.
  // This is the last chance for read to occur in the WriteTransaction
  // that's closing. This special ReadTransaction will not close the
  // underlying transaction.
  ReadTransaction read_trans(GetUserShare(), trans);

  ModelTypeSet models_with_changes;
  for (ChangeRecordMap::const_iterator it = change_records_.begin();
       it != change_records_.end(); ++it) {
    DCHECK(!it->second.Get().empty());
    ModelType type = syncer::ModelTypeFromInt(it->first);
    change_delegate_->OnChangesApplied(
        type, trans->directory()->GetTransactionVersion(type), &read_trans,
        it->second);
    change_observer_.Call(FROM_HERE,
                          &SyncManager::ChangeObserver::OnChangesApplied, type,
                          write_transaction_info.Get().id, it->second);
    models_with_changes.Put(type);
  }
  change_records_.clear();
  return models_with_changes;
}

void BraveSyncManagerImpl::HandleCalculateChangesChangeEventFromSyncApi(
    const ImmutableWriteTransactionInfo& write_transaction_info,
    syncer::syncable::BaseTransaction* trans,
    std::vector<int64_t>* entries_changed) {
  // We have been notified about a user action changing a sync model.
  LOG_IF(WARNING, !change_records_.empty())
      << "CALCULATE_CHANGES called with unapplied old changes.";

  // The mutated model type, or UNSPECIFIED if nothing was mutated.
  ModelTypeSet mutated_model_types;

  const ImmutableEntryKernelMutationMap& mutations =
      write_transaction_info.Get().mutations;
  for (auto it = mutations.Get().begin(); it != mutations.Get().end(); ++it) {
    if (!it->second.mutated.ref(syncer::syncable::IS_UNSYNCED)) {
      continue;
    }

    ModelType model_type =
        GetModelTypeFromSpecifics(it->second.mutated.ref(SPECIFICS));
    if (model_type < FIRST_REAL_MODEL_TYPE) {
      NOTREACHED() << "Permanent or underspecified item changed via syncapi.";
      continue;
    }

    // Found real mutation.
    if (model_type != syncer::UNSPECIFIED) {
      mutated_model_types.Put(model_type);
      entries_changed->push_back(it->second.mutated.ref(
                                    syncer::syncable::META_HANDLE));
    }
  }
}

void BraveSyncManagerImpl::HandleCalculateChangesChangeEventFromSyncer(
    const ImmutableWriteTransactionInfo& write_transaction_info,
    syncer::syncable::BaseTransaction* trans,
    std::vector<int64_t>* entries_changed) {
  // We only expect one notification per sync step, so change_buffers_ should
  // contain no pending entries.
  LOG_IF(WARNING, !change_records_.empty())
      << "CALCULATE_CHANGES called with unapplied old changes.";

  syncer::ChangeReorderBuffer change_buffers[MODEL_TYPE_COUNT];

  // Cryptographer* crypto = directory()->GetCryptographer(trans);
  const ImmutableEntryKernelMutationMap& mutations =
      write_transaction_info.Get().mutations;
  for (auto it = mutations.Get().begin(); it != mutations.Get().end(); ++it) {
    bool existed_before = !it->second.original.ref(syncer::syncable::IS_DEL);
    bool exists_now = !it->second.mutated.ref(syncer::syncable::IS_DEL);

    // Omit items that aren't associated with a model.
    ModelType type =
        GetModelTypeFromSpecifics(it->second.mutated.ref(SPECIFICS));
    if (type < FIRST_REAL_MODEL_TYPE)
      continue;

    int64_t handle = it->first;
    if (exists_now && !existed_before)
      change_buffers[type].PushAddedItem(handle);
    else if (!exists_now && existed_before)
      change_buffers[type].PushDeletedItem(handle);
    // else if (exists_now && existed_before &&
    //          VisiblePropertiesDiffer(it->second, crypto))
    //   change_buffers[type].PushUpdatedItem(handle);
  }

  ReadTransaction read_trans(GetUserShare(), trans);
  for (int i = FIRST_REAL_MODEL_TYPE; i < MODEL_TYPE_COUNT; ++i) {
    if (!change_buffers[i].IsEmpty()) {
      if (change_buffers[i].GetAllChangesInTreeOrder(&read_trans,
                                                     &(change_records_[i]))) {
        for (size_t j = 0; j < change_records_[i].Get().size(); ++j)
          entries_changed->push_back((change_records_[i].Get())[j].id);
      }
      if (change_records_[i].Get().empty())
        change_records_.erase(i);
    }
  }
}

void BraveSyncManagerImpl::RequestNudgeForDataTypes(
    const base::Location& nudge_location,
    ModelTypeSet types) {
  debug_info_event_listener_.OnNudgeFromDatatype(*(types.begin()));

  scheduler_->ScheduleLocalNudge(types, nudge_location);
}

void BraveSyncManagerImpl::NudgeForInitialDownload(ModelType type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  scheduler_->ScheduleInitialSyncNudge(type);
}

void BraveSyncManagerImpl::NudgeForCommit(ModelType type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  RequestNudgeForDataTypes(FROM_HERE, ModelTypeSet(type));
}

void BraveSyncManagerImpl::NudgeForRefresh(ModelType type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  RefreshTypes(ModelTypeSet(type));
}

void BraveSyncManagerImpl::SetJsEventHandler(
    const WeakHandle<syncer::JsEventHandler>& event_handler) {
  js_sync_manager_observer_.SetJsEventHandler(event_handler);
  js_mutation_event_observer_.SetJsEventHandler(event_handler);
  js_sync_encryption_handler_observer_.SetJsEventHandler(event_handler);
}

void BraveSyncManagerImpl::SetInvalidatorEnabled(bool invalidator_enabled) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  DVLOG(1) << "Invalidator enabled state is now: " << invalidator_enabled;
  allstatus_.SetNotificationsEnabled(invalidator_enabled);
  scheduler_->SetNotificationsEnabled(invalidator_enabled);
}

void BraveSyncManagerImpl::OnIncomingInvalidation(
    ModelType type,
    std::unique_ptr<syncer::InvalidationInterface> invalidation) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  allstatus_.IncrementNotificationsReceived();
  scheduler_->ScheduleInvalidationNudge(type, std::move(invalidation),
                                        FROM_HERE);
}

void BraveSyncManagerImpl::RefreshTypes(ModelTypeSet types) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (types.Empty()) {
    LOG(WARNING) << "Sync received refresh request with no types specified.";
  } else {
    scheduler_->ScheduleLocalRefreshRequest(types, FROM_HERE);
  }
}

SyncStatus BraveSyncManagerImpl::GetDetailedStatus() const {
  return allstatus_.status();
}

void BraveSyncManagerImpl::SaveChanges() {
  directory()->SaveChanges();
}

UserShare* BraveSyncManagerImpl::GetUserShare() {
  DCHECK(initialized_);
  return &share_;
}

ModelTypeConnector* BraveSyncManagerImpl::GetModelTypeConnector() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return model_type_registry_.get();
}

std::unique_ptr<ModelTypeConnector>
BraveSyncManagerImpl::GetModelTypeConnectorProxy() {
  DCHECK(initialized_);
  return std::make_unique<syncer::ModelTypeConnectorProxy>(
      base::SequencedTaskRunnerHandle::Get(),
      model_type_registry_->AsWeakPtr());
}

const std::string BraveSyncManagerImpl::cache_guid() {
  DCHECK(initialized_);
  return directory()->cache_guid();
}

bool BraveSyncManagerImpl::ReceivedExperiment(Experiments* experiments) {
  ReadTransaction trans(FROM_HERE, GetUserShare());
  syncer::ReadNode nigori_node(&trans);
  if (nigori_node.InitTypeRoot(syncer::NIGORI) != syncer::BaseNode::INIT_OK) {
    DVLOG(1) << "Couldn't find Nigori node.";
    return false;
  }
  bool found_experiment = false;

  syncer::ReadNode favicon_sync_node(&trans);
  if (favicon_sync_node.InitByClientTagLookup(syncer::EXPERIMENTS,
                                              syncer::kFaviconSyncTag) ==
      syncer::BaseNode::INIT_OK) {
    experiments->favicon_sync_limit =
        favicon_sync_node.GetExperimentsSpecifics()
            .favicon_sync()
            .favicon_sync_limit();
    found_experiment = true;
  }

  return found_experiment;
}

bool BraveSyncManagerImpl::HasUnsyncedItemsForTest() {
  return model_type_registry_->HasUnsyncedItems();
}

SyncEncryptionHandler* BraveSyncManagerImpl::GetEncryptionHandler() {
  return sync_encryption_handler_.get();
}

std::vector<std::unique_ptr<syncer::ProtocolEvent>>
BraveSyncManagerImpl::GetBufferedProtocolEvents() {
  return protocol_event_buffer_.GetBufferedProtocolEvents();
}

void BraveSyncManagerImpl::RegisterDirectoryTypeDebugInfoObserver(
    TypeDebugInfoObserver* observer) {
  model_type_registry_->RegisterDirectoryTypeDebugInfoObserver(observer);
}

void BraveSyncManagerImpl::UnregisterDirectoryTypeDebugInfoObserver(
    TypeDebugInfoObserver* observer) {
  model_type_registry_->UnregisterDirectoryTypeDebugInfoObserver(observer);
}

bool BraveSyncManagerImpl::HasDirectoryTypeDebugInfoObserver(
    TypeDebugInfoObserver* observer) {
  return model_type_registry_->HasDirectoryTypeDebugInfoObserver(observer);
}

void BraveSyncManagerImpl::RequestEmitDebugInfo() {
  model_type_registry_->RequestEmitDebugInfo();
}

void BraveSyncManagerImpl::ClearServerData(const base::Closure& callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  scheduler_->Start(SyncScheduler::CLEAR_SERVER_DATA_MODE, base::Time());
  syncer::ClearParams params(callback);
  scheduler_->ScheduleClearServerData(params);
}

void BraveSyncManagerImpl::OnCookieJarChanged(bool account_mismatch,
                                         bool empty_jar) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  cycle_context_->set_cookie_jar_mismatch(account_mismatch);
  cycle_context_->set_cookie_jar_empty(empty_jar);
}

void BraveSyncManagerImpl::OnMemoryDump(
    base::trace_event::ProcessMemoryDump* pmd) {
  directory()->OnMemoryDump(pmd);
}

void BraveSyncManagerImpl::UpdateInvalidationClientId(
    const std::string& client_id) {
  DVLOG(1) << "Setting invalidator client ID: " << client_id;
  allstatus_.SetInvalidatorClientId(client_id);
}

}  // namespace brave_sync
