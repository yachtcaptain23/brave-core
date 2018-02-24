/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef BRAVE_BROWSER_UI_WEBUI_WEBUI_UTIL_H_
#define BRAVE_BROWSER_UI_WEBUI_WEBUI_UTIL_H_

#include <vector>

namespace content {
class WebUIDataSource;
}

struct WebUISimpleItem {
  const char* name;
  int id;
};

void AddLocalizedStringsBulk(content::WebUIDataSource* html_source,
                             const std::vector<WebUISimpleItem>& simple_items);

void AddResourcePaths(content::WebUIDataSource* html_source,
                      const std::vector<WebUISimpleItem>& simple_items);


#endif  // BRAVE_BROWSER_UI_WEBUI_WEBUI_UTIL_H_

