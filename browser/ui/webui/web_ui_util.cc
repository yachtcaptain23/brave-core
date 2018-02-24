/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "brave/browser/ui/webui/web_ui_util.h"

#include "content/public/browser/web_ui_data_source.h"


void AddLocalizedStringsBulk(content::WebUIDataSource* html_source,
                             const std::vector<WebUISimpleItem>& simple_items) {
  for (size_t i = 0; i < simple_items.size(); i++) {
    html_source->AddLocalizedString(simple_items[i].name,
                                    simple_items[i].id);
  }
}

void AddResourcePaths(content::WebUIDataSource* html_source,
                      const std::vector<WebUISimpleItem>& simple_items) {
  for (size_t i = 0; i < simple_items.size(); i++) {
    html_source->AddResourcePath(simple_items[i].name,
                                 simple_items[i].id);
  }
}
