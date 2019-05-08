/* Copyright (c) 2019 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef BRAVE_COMPONENTS_BRAVE_SHIELDS_BROWSER_SITE_SPECIFIC_SCRIPT_SERVICE_H_
#define BRAVE_COMPONENTS_BRAVE_SHIELDS_BROWSER_SITE_SPECIFIC_SCRIPT_SERVICE_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/sequenced_task_runner.h"
#include "base/values.h"
#include "brave/components/brave_shields/browser/base_local_data_files_observer.h"
#include "brave/components/brave_shields/browser/dat_file_util.h"
#include "content/public/common/resource_type.h"
#include "extensions/common/url_pattern_set.h"
#include "url/gurl.h"

#define SITE_SPECIFIC_SCRIPT_CONFIG_FILE "SiteSpecificScripts.json"
#define SITE_SPECIFIC_SCRIPT_CONFIG_FILE_VERSION "1"

class SiteSpecificScriptServiceTest;

namespace brave_shields {

class SiteSpecificScriptRule {
 public:
  SiteSpecificScriptRule(base::ListValue* urls_value,
                         base::ListValue* scripts_value,
                         const base::FilePath& root_dir);
  ~SiteSpecificScriptRule();

  bool MatchesURL(const GURL& url) const;
  void Populate(std::vector<std::string>* scripts) const;

 private:
  scoped_refptr<base::SequencedTaskRunner> GetTaskRunner();
  void AddScriptAfterLoad(std::unique_ptr<std::string> contents,
                          bool did_load);

  extensions::URLPatternSet urls_;
  std::vector<std::string> scripts_;
  base::WeakPtrFactory<SiteSpecificScriptRule> weak_factory_;
  DISALLOW_COPY_AND_ASSIGN(SiteSpecificScriptRule);
};

// The brave shields service in charge of site-specific script injection
class SiteSpecificScriptService : public BaseLocalDataFilesObserver {
 public:
  SiteSpecificScriptService();
  ~SiteSpecificScriptService() override;

  bool ScriptsFor(const GURL& primary_url, std::vector<std::string>* scripts);

  scoped_refptr<base::SequencedTaskRunner> GetTaskRunner();

  // implementation of BaseLocalDataFilesObserver
  void OnComponentReady(const std::string& component_id,
                        const base::FilePath& install_dir,
                        const std::string& manifest) override;

 private:
  friend class ::SiteSpecificScriptServiceTest;

  void OnDATFileDataReady();
  void LoadOnTaskRunner();

  std::string file_contents_;
  std::vector<std::unique_ptr<SiteSpecificScriptRule>> rules_;
  base::FilePath install_dir_;

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<SiteSpecificScriptService> weak_factory_;
  DISALLOW_COPY_AND_ASSIGN(SiteSpecificScriptService);
};

// Creates the SiteSpecificScriptService
std::unique_ptr<SiteSpecificScriptService> SiteSpecificScriptServiceFactory();

}  // namespace brave_shields

#endif  // BRAVE_COMPONENTS_BRAVE_SHIELDS_BROWSER_SITE_SPECIFIC_SCRIPT_SERVICE_H_
