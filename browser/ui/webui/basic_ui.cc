/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "brave/browser/ui/webui/basic_ui.h"

#include "brave/common/webui_url_constants.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/url_constants.h"
#include "components/grit/brave_components_resources.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "ui/base/resource/resource_bundle.h"




#include "base/strings/utf_string_conversions.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/browser/dom_storage/dom_storage_area.h"
#include "content/browser/dom_storage/dom_storage_context_wrapper.h"
#include "content/browser/dom_storage/dom_storage_namespace.h"
#include "content/common/dom_storage/dom_storage_types.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/browser/profiles/profile_manager.h"

using content::WebContents;
using content::WebUIMessageHandler;

namespace {

content::WebUIDataSource* CreateBasicUIHTMLSource(const std::string& name,
                                                  const std::string& js_file,
                                                  int js_resource_id,
                                                  int html_resource_id) {
  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(name);
  source->AddResourcePath(js_file, js_resource_id);
  source->AddResourcePath("af7ae505a9eed503f8b8e6982036873e.woff2", IDR_BRAVE_COMMON_FONT_AWESOME_1);
  source->AddResourcePath("fee66e712a8a08eef5805a46892932ad.woff", IDR_BRAVE_COMMON_FONT_AWESOME_2);
  source->AddResourcePath("b06871f281fee6b241d60582ae9369b9.ttf", IDR_BRAVE_COMMON_FONT_AWESOME_3);
  source->AddResourcePath("img/toolbar/menu_btn.svg", IDR_BRAVE_COMMON_TOOLBAR_IMG);

  source->AddLocalizedString("adsBlocked", IDS_BRAVE_NEW_TAB_TOTAL_ADS_BLOCKED);
  source->AddLocalizedString("trackersBlocked", IDS_BRAVE_NEW_TAB_TOTAL_TRACKERS_BLOCKED);
  source->AddLocalizedString("httpsUpgraded", IDS_BRAVE_NEW_TAB_TOTAL_HTTPS_UPGRADES);
  source->AddLocalizedString("estimatedTimeSaved", IDS_BRAVE_NEW_TAB_TOTAL_TIME_SAVED);
  source->AddLocalizedString("thumbRemoved", IDS_BRAVE_NEW_TAB_THUMB_REMOVED);
  source->AddLocalizedString("undoRemoved", IDS_BRAVE_NEW_TAB_UNDO_REMOVED);
  source->AddLocalizedString("restoreAll", IDS_BRAVE_NEW_TAB_RESTORE_ALL);

  // Hash path is the MD5 of the file contents, webpack image loader does this
  source->AddResourcePath("fd85070af5114d6ac462c466e78448e4.svg", IDR_BRAVE_NEW_TAB_IMG1);
  source->AddResourcePath("314e7529efec41c8867019815f4d8dad.svg", IDR_BRAVE_NEW_TAB_IMG4);
  source->AddResourcePath("6c337c63662ee0ba4e57f6f8156d69ce.svg", IDR_BRAVE_NEW_TAB_IMG2);
  source->AddResourcePath("50cc52a4f1743ea74a21da996fe44272.jpg", IDR_BRAVE_NEW_TAB_IMG14);

  source->SetDefaultResource(html_resource_id);
  source->SetJsonPath("strings.js");

  return source;
}

// The handler for Javascript messages for Brave about: pages
class BasicDOMHandler : public WebUIMessageHandler {
 public:
  BasicDOMHandler() {
  }
  ~BasicDOMHandler() override {}

  void Init();

  // WebUIMessageHandler implementation.
  void RegisterMessages() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(BasicDOMHandler);
};

void BasicDOMHandler::RegisterMessages() {
}

void BasicDOMHandler::Init() {
}

}  // namespace

BasicUI::BasicUI(content::WebUI* web_ui,
    const std::string& name,
    const std::string& js_file,
    int js_resource_id,
    int html_resource_id)
    : WebUIController(web_ui) {



  ////////////////////////////////////////
  // THIS IS JUST A TEST, CODE IS NOT STAYING HERE
  ////////////////////////////////////////

  Profile* profile = Profile::FromWebUI(web_ui);
  GURL new_tab = GURL(chrome::kChromeUINewTabURL).GetOrigin();

  content::StoragePartition* partition =
    content::BrowserContext::GetStoragePartitionForSite(
      profile, new_tab);

LOG(ERROR) << "--------------1";
  content::DOMStorageContextWrapper* storage_context_wrapper =
    static_cast<content::DOMStorageContextWrapper*>(
      partition->GetDOMStorageContext());

  LOG(ERROR) << "--------------2";
  content::DOMStorageContextImpl* context = storage_context_wrapper->context();

  LOG(ERROR) << "--------------3";
  content::DOMStorageNamespace* storage_namespace = context->
    GetStorageNamespace(content::kLocalStorageNamespaceId);
  LOG(ERROR) << "--------------4";

  content::DOMStorageArea* area = storage_namespace->GetOpenStorageArea(new_tab);
  bool was_opened = false;
  if (!area) {
    LOG(ERROR) << "---Opening";
    area = storage_namespace->OpenStorageArea(new_tab);
    was_opened = true;
  }
  if (area) {

    base::NullableString16 str1 = area->Key(0);
    LOG(ERROR) << "str1 is: " << base::UTF16ToUTF8(str1.string());
    base::NullableString16 str2 = area->Key(1);
    LOG(ERROR) << "str2 is: " << base::UTF16ToUTF8(str2.string());

    LOG(ERROR) << "--------------5.1";
    base::NullableString16 str = area->GetItem(base::UTF8ToUTF16("new-tab-data"));
    LOG(ERROR) << "str is: " << base::UTF16ToUTF8(str.string());

    base::NullableString16 client_old_value;
    base::NullableString16 old_value;
    area->SetItem(base::UTF8ToUTF16("new-tab-data-3"), base::UTF8ToUTF16("test"), client_old_value, &old_value);
  } else {
    LOG(ERROR) << "--------------5.2";
  }
  if (was_opened && area) {
    LOG(ERROR) << "---Closing";
    storage_namespace->CloseStorageArea(area);
  }

  ////////////////////////////////////////
  // END THIS IS JUST A TEST, CODE IS NOT STAYING HERE
  ////////////////////////////////////////


  auto handler_owner = base::MakeUnique<BasicDOMHandler>();
  BasicDOMHandler* handler = handler_owner.get();
  web_ui->AddMessageHandler(std::move(handler_owner));
  handler->Init();
  content::WebUIDataSource::Add(profile,
      CreateBasicUIHTMLSource(name, js_file, js_resource_id, html_resource_id));
}
