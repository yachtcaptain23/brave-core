/* Copyright (c) 2020 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "bat/ads/internal/frequency_capping/exclusion_rules/day_parting_frequency_cap.h"

#include <string>
#include <vector>

#include "base/strings/stringprintf.h"
#include "base/strings/string_number_conversions.h"
#include "bat/ads/internal/ads_impl.h"
#include "bat/ads/internal/frequency_capping/frequency_capping_util.h"

namespace ads {

DayPartingFrequencyCap::DayPartingFrequencyCap(
    const AdsImpl* const ads)
    : ads_(ads) {
  DCHECK(ads_);
}

DayPartingFrequencyCap::~DayPartingFrequencyCap() = default;

bool DayPartingFrequencyCap::ShouldExclude(
    const CreativeAdInfo& ad) {
  if (!DoesRespectCap(ad)) {
    last_message_ = base::StringPrintf("creativeSetId %s excluded as not "
        "within the scheduled timeslot", ad.creative_set_id.c_str());

    return true;
  }

  return false;
}

std::string DayPartingFrequencyCap::get_last_message() const {
  return last_message_;
}

bool DayPartingFrequencyCap::DoesRespectCap(
    const CreativeAdInfo& ad) const {
  // If there's no day part specified, let it be displayed
  if (ad.day_parts.empty()) {
    return true;
  }

  std::string current_dow = DayPartingFrequencyCap::GetCurrentDayOfWeek();
  std::string days_of_week;
  uint64_t current_minutes_from_start =
    DayPartingFrequencyCap::GetCurrentLocalMinutesFromStart();
  uint64_t start_time;
  uint64_t end_time;
  std::vector<std::string> parsed_day_part;

  for (const std::string& day_part : DayPartingFrequencyCap::ParseDayPartList(ad.day_parts)) {
      parsed_day_part = DayPartingFrequencyCap::ParseDayPart(day_part);
      days_of_week = parsed_day_part[0];
      start_time = std::stoi(parsed_day_part[1]);
      end_time = std::stoi(parsed_day_part[2]);

      if (DayPartingFrequencyCap::HasDayOfWeekMatch(current_dow, days_of_week)
          && DayPartingFrequencyCap::HasTimeSlotMatch(
              current_minutes_from_start,
              start_time,
              end_time)) {
          return true;
      }
  }
  return false;
}

bool DayPartingFrequencyCap::HasDayOfWeekMatch(
    const std::string& current_dow,
    const std::string& days_of_week) const {
  return days_of_week.find(current_dow) != std::string::npos;
}

bool DayPartingFrequencyCap::HasTimeSlotMatch(
    const uint64_t current_minutes_from_start,
    const uint64_t start_time,
    const uint64_t end_time) const {

    return start_time <= current_minutes_from_start &&
      current_minutes_from_start <= end_time;
}

std::vector<std::string> DayPartingFrequencyCap::ParseDayPart(
    std::string day_part) const {
  std::vector<std::string> list;
  size_t pos = 0;
  std::string token;
  std::string delimiter = "_";

  while ((pos = day_part.find(delimiter)) != std::string::npos) {
      token = day_part.substr(0, pos);
      list.push_back(token);
      day_part.erase(0, pos + delimiter.length());
  }
  list.push_back(day_part);
  return list;
}

std::vector<std::string> DayPartingFrequencyCap::ParseDayPartList(
    std::string day_parts) const {
  std::vector<std::string> list;
  size_t pos = 0;
  std::string token;
  std::string delimiter = ",";

  while ((pos = day_parts.find(delimiter)) != std::string::npos) {
      token = day_parts.substr(0, pos);
      list.push_back(token);
      day_parts.erase(0, pos + delimiter.length());
  }
  if (!day_parts.empty()) {
    list.push_back(day_parts);
  }
  return list;
}

std::string DayPartingFrequencyCap::GetCurrentDayOfWeek() const {
  auto now = base::Time::Now();
  base::Time::Exploded exploded;
  now.LocalExplode(&exploded);
  return base::NumberToString(exploded.day_of_week);
}

uint64_t DayPartingFrequencyCap::GetCurrentLocalMinutesFromStart() const {
  auto now = base::Time::Now();
  base::Time::Exploded exploded;
  now.LocalExplode(&exploded);
  return base::Time::kMinutesPerHour * exploded.hour + exploded.minute;
}

}  // namespace ads
