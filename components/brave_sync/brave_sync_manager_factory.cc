/* Copyright (c) 2019 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "brave/components/brave_sync/brave_sync_manager_factory.h"

#include "brave/components/brave_sync/brave_sync_manager_impl.h"

namespace brave_sync {

BraveSyncManagerFactory::BraveSyncManagerFactory(
    network::NetworkConnectionTracker* network_connection_tracker)
    : syncer::SyncManagerFactory(network_connection_tracker),
      network_connection_tracker_(network_connection_tracker) {}

BraveSyncManagerFactory::~BraveSyncManagerFactory() {}

std::unique_ptr<syncer::SyncManager> BraveSyncManagerFactory::CreateSyncManager(
    const std::string& name) {
  return std::make_unique<BraveSyncManagerImpl>(name, network_connection_tracker_);
}

}   // namespace brave_sync
