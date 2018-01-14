/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "brave/browser/net/profile_static_redirect_network_delegate_helper.h"

#include <map>
#include <string>

#include "brave/components/component_updater/component_updater_url_constants.h"
#include "components/component_updater/component_updater_url_constants.h"
#include "extensions/common/url_pattern.h"
#include "net/url_request/url_request.h"





namespace brave {

URLPattern updateServerPattern(URLPattern::SCHEME_HTTP | URLPattern::SCHEME_HTTPS,
    std::string(component_updater::kBraveUpdaterDefaultUrl) + "*");

auto httpMask = URLPattern::SCHEME_HTTP | URLPattern::SCHEME_HTTPS;
std::map<URLPattern, std::string> httpPatternMap = {
  { URLPattern(httpMask, std::string(component_updater::kUpdaterDefaultUrl)+ "*"), component_updater::kBraveUpdaterDefaultUrl },
  { URLPattern(httpMask, std::string(component_updater::kUpdaterFallbackUrl)+ "*"), component_updater::kBraveUpdaterDefaultUrl },
  { URLPattern(httpMask, std::string(component_updater::kUpdaterDefaultUrlAlt)+ "*"), component_updater::kBraveUpdaterDefaultUrl },
  { URLPattern(httpMask, std::string(component_updater::kUpdaterFallbackUrlAlt)+ "*"), component_updater::kBraveUpdaterDefaultUrl },
};

int OnBeforeURLRequest_ProfileStaticRedirectWork(
    net::URLRequest* request,
    GURL* new_url,
    const ResponseCallback& next_callback,
    std::shared_ptr<OnBeforeURLRequestContext> ctx) {
  for (auto& key_value : httpPatternMap) {
    if (key_value.first.MatchesURL(request->url())) {
      *new_url = GURL(key_value.second + "?" + request->url().query());
      LOG(ERROR) << "URL: " << request->url() << "\n, matching pattern:\n" << key_value.first << "\n changing to:\n" << key_value.second + "?" + request->url().query();
    }
  }
  return net::OK;
}

}  // namespace brave
