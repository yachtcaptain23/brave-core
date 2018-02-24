/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "brave/browser/ui/webui/brave_payments_html_source.h"

#include <string>
#include "brave/browser/ui/webui/web_ui_util.h"
#include "brave/common/pref_names.h"
#include "chrome/browser/profiles/profile.h"
#include "components/grit/brave_components_resources.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"

void CustomizeBravePaymentsHTMLSource(Profile* profile, content::WebUIDataSource* source) {
  std::vector<WebUISimpleItem> resources = {
  };
  AddResourcePaths(source, resources);

  std::vector<WebUISimpleItem> localized_strings = {
    { "title", IDS_BRAVE_PAYMENTS_TITLE}
  };
  AddLocalizedStringsBulk(source, localized_strings);
  }

void CustomizeBravePaymentsWebUIProperties(content::WebUI* web_ui,
    Profile* profile, content::RenderViewHost* render_view_host) {
}
