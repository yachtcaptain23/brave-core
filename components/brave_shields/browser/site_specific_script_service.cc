/* Copyright (c) 2019 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "brave/components/brave_shields/browser/site_specific_script_service.h"

#include <algorithm>
#include <utility>

#include "base/base_paths.h"
#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_restrictions.h"
#include "brave/browser/brave_browser_process_impl.h"
#include "brave/components/brave_shields/browser/ad_block_service.h"
#include "brave/components/brave_shields/browser/local_data_files_service.h"
#include "brave/components/brave_shields/browser/dat_file_util.h"

namespace brave_shields {

SiteSpecificScriptRule::SiteSpecificScriptRule(base::ListValue* urls_value,
                                               base::ListValue* scripts_value,
                                               const base::FilePath& root_dir) {
  std::string error;
  if (!urls_.Populate(*urls_value,
                      URLPattern::SCHEME_HTTP|URLPattern::SCHEME_HTTPS,
                      /* allow_file_access */ false,
                      &error)) {
    LOG(ERROR) << "Malformed URL patterns in site-specific script configuration";
    return;
  }
  for (const auto& scripts_it : scripts_value->GetList()) {
    base::FilePath script_path = install_dir.AppendASCII(
      SITE_SPECIFIC_SCRIPT_CONFIG_FILE_VERSION).AppendASCII(
        scripts_it.GetString());
    if (script_path.ReferencesParent()) {
      LOG(ERROR) << "Malformed filenames in site-specific script configuration";
    } else {
      std::string contents;
      if (base::ReadFileToString(script_path, &contents)) {
        scripts_.push_back(contents);
      }
    }
  }
}

SiteSpecificScriptRule::~SiteSpecificScriptRule() = default;

bool SiteSpecificScriptRule::MatchesURL(const GURL& url) const {
  return urls_.MatchesURL(url);
}

void SiteSpecificScriptRule::Populate(std::vector<std::string>* scripts) const {
  *scripts = scripts_;
}

SiteSpecificScriptService::SiteSpecificScriptService()
    : weak_factory_(this) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

SiteSpecificScriptService::~SiteSpecificScriptService() {
}

bool SiteSpecificScriptService::ScriptsFor(const GURL& primary_url,
                                           std::vector<std::string>* scripts) {
  bool any = false;
  scripts->clear();
  for (const auto& rule : rules_) {
    if (rule->MatchesURL(primary_url)) {
      rule->Populate(scripts);
      any = true;
      break;
    }
  }
  return any;
}

void SiteSpecificScriptService::OnDATFileDataReady() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  rules_.clear();
  if (file_contents_.empty()) {
    LOG(ERROR) << "Could not obtain site-specific script configuration";
    return;
  }
  base::Optional<base::Value> root = base::JSONReader::Read(file_contents_);
  file_contents_.clear();
  if (!root) {
    LOG(ERROR) << "Failed to parse site-specific script configuration";
    return;
  }
  base::ListValue* root_list = nullptr;
  root->GetAsList(&root_list);
  for (base::Value& rule_it : root_list->GetList()) {
    base::DictionaryValue* rule_dict = nullptr;
    rule_it.GetAsDictionary(&rule_dict);
    base::ListValue* urls_value = nullptr;
    rule_dict->GetList("urls", &urls_value);
    base::ListValue* scripts_value = nullptr;
    rule_dict->GetList("scripts", &scripts_value);
    std::unique_ptr<SiteSpecificScriptRule> rule =
      std::make_unique<SiteSpecificScriptRule>(urls_value,
                                               scripts_value,
                                               install_dir_);
    rules_.push_back(std::move(rule));
  }
}

void SiteSpecificScriptService::OnComponentReady(
    const std::string& component_id,
    const base::FilePath& install_dir,
    const std::string& manifest) {
  install_dir_ = install_dir;
  base::FilePath dat_file_path = install_dir.AppendASCII(
    SITE_SPECIFIC_SCRIPT_CONFIG_FILE_VERSION).AppendASCII(
      SITE_SPECIFIC_SCRIPT_CONFIG_FILE);
  GetTaskRunner()->PostTaskAndReply(
      FROM_HERE,
      base::Bind(&GetDATFileAsString, dat_file_path, &file_contents_),
      base::Bind(&SiteSpecificScriptService::OnDATFileDataReady,
                 weak_factory_.GetWeakPtr()));
}

scoped_refptr<base::SequencedTaskRunner>
  SiteSpecificScriptService::GetTaskRunner() {
  // We share the same task runner as ad-block code
  return g_brave_browser_process->ad_block_service()->GetTaskRunner();
}

///////////////////////////////////////////////////////////////////////////////

// The site-specific script factory. Using the Brave Shields as a singleton
// is the job of the browser process.
std::unique_ptr<SiteSpecificScriptService>
SiteSpecificScriptServiceFactory() {
  std::unique_ptr<SiteSpecificScriptService> service =
    std::make_unique<SiteSpecificScriptService>();
  g_brave_browser_process->local_data_files_service()->AddObserver(
    service.get());
  return service;
}

}  // namespace brave_shields
