/* Copyright (c) 2019 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef BRAVE_COMPONENTS_BRAVE_SYNC_BRAVE_SYNC_MANAGER_FACTORY_H_
#define BRAVE_COMPONENTS_BRAVE_SYNC_BRAVE_SYNC_MANAGER_FACTORY_H_

#include "components/sync/engine/sync_manager_factory.h"

namespace brave_sync {

class BraveSyncManagerFactory : public syncer::SyncManagerFactory {
 public:
   explicit BraveSyncManagerFactory(
      network::NetworkConnectionTracker* network_connection_tracker);
   ~BraveSyncManagerFactory() override;

   std::unique_ptr<syncer::SyncManager> CreateSyncManager(
      const std::string& name) override;
 private:
    network::NetworkConnectionTracker* network_connection_tracker_;

    DISALLOW_COPY_AND_ASSIGN(BraveSyncManagerFactory);
};

}   // namespace brave_sync

#endif  // BRAVE_COMPONENTS_BRAVE_SYNC_BRAVE_SYNC_MANAGER_FACTORY_H_
