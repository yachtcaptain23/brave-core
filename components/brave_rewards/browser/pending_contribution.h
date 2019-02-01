/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef BRAVE_BROWSER_PAYMENTS_PENDING_CONTRIBUTION_
#define BRAVE_BROWSER_PAYMENTS_PENDING_CONTRIBUTION_

#include <string>

namespace brave_rewards {

struct PendingContributionInfo {
  PendingContributionInfo();
  ~PendingContributionInfo();
  PendingContributionInfo(const PendingContributionInfo& data);

  std::string publisher_key;
  double percentage = 0.0;
  bool verified;
  int excluded;
  std::string name;
  std::string favicon_url;
  std::string url;
  std::string provider;
  double amount = 0;
  uint32_t added_date = 0;
  std::string viewing_id;
  int category;
};

using PendingContributionInfoList = std::vector<PendingContributionInfo>;

}  // namespace brave_rewards

#endif //BRAVE_BROWSER_PAYMENTS_PENDING_CONTRIBUTION_
