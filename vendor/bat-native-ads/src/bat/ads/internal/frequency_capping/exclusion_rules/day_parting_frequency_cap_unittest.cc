/* Copyright (c) 2020 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "bat/ads/internal/frequency_capping/exclusion_rules/day_parting_frequency_cap.h"

#include <memory>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/task_environment.h"
#include "brave/components/l10n/browser/locale_helper_mock.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "bat/ads/internal/ads_client_mock.h"
#include "bat/ads/internal/ads_impl.h"
#include "bat/ads/internal/bundle/creative_ad_info.h"
#include "bat/ads/internal/frequency_capping/frequency_capping_unittest_util.h"
#include "bat/ads/internal/platform/platform_helper_mock.h"
#include "bat/ads/internal/unittest_util.h"
#include "bat/ads/internal/time_util.h"

// npm run test -- brave_unit_tests --filter=BatAds*

using ::testing::NiceMock;
using ::testing::Return;

namespace ads {

namespace {

const char kCreativeSetId[] = "654f10df-fbc4-4a92-8d43-2edf73734a60";

}  // namespace

class BatAdsDayPartingFrequencyCapTest : public ::testing::Test {
 protected:
  BatAdsDayPartingFrequencyCapTest()
      : ads_client_mock_(std::make_unique<NiceMock<AdsClientMock>>()),
        ads_(std::make_unique<AdsImpl>(ads_client_mock_.get())),
        locale_helper_mock_(std::make_unique<
            NiceMock<brave_l10n::LocaleHelperMock>>()),
        platform_helper_mock_(std::make_unique<
            NiceMock<PlatformHelperMock>>()),
        frequency_cap_(std::make_unique<
            DayPartingFrequencyCap>(ads_.get())) {
    // You can do set-up work for each test here

    PlatformHelper::GetInstance()->set_for_testing(platform_helper_mock_.get());
  }

  ~BatAdsDayPartingFrequencyCapTest() override {
    // You can do clean-up work that doesn't throw exceptions here
  }

  // If the constructor and destructor are not enough for setting up and
  // cleaning up each test, you can use the following methods

  void SetUp() override {
    // Code here will be called immediately after the constructor (right before
    // each test)

    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    const base::FilePath path = temp_dir_.GetPath();

    SetBuildChannel(false, "test");

    ON_CALL(*locale_helper_mock_, GetLocale())
        .WillByDefault(Return("en-US"));

    MockPlatformHelper(platform_helper_mock_, PlatformType::kMacOS);

    ads_->OnWalletUpdated("c387c2d8-a26d-4451-83e4-5c0c6fd942be",
        "5BEKM1Y7xcRSg/1q8in/+Lki2weFZQB+UMYZlRw8ql8=");

    MockLoad(ads_client_mock_);
    MockLoadUserModelForId(ads_client_mock_);
    MockLoadResourceForId(ads_client_mock_);
    MockSave(ads_client_mock_);

    database_ = std::make_unique<Database>(path.AppendASCII("database.sqlite"));
    MockRunDBTransaction(ads_client_mock_, database_);

    Initialize(ads_);
  }

  void TearDown() override {
    // Code here will be called immediately after each test (right before the
    // destructor)
  }

  // Objects declared here can be used by all tests in the test case

  base::test::TaskEnvironment task_environment_;

  base::ScopedTempDir temp_dir_;

  std::unique_ptr<AdsClientMock> ads_client_mock_;
  std::unique_ptr<AdsImpl> ads_;
  std::unique_ptr<brave_l10n::LocaleHelperMock> locale_helper_mock_;
  std::unique_ptr<PlatformHelperMock> platform_helper_mock_;
  std::unique_ptr<DayPartingFrequencyCap> frequency_cap_;
  std::unique_ptr<Database> database_;
};

TEST_F(BatAdsDayPartingFrequencyCapTest, AllowIfDayPartsIsEmpty) {
  // Arrange
  CreativeAdInfo ad;
  ad.creative_set_id = kCreativeSetId;

  // Act
  const bool should_exclude = frequency_cap_->ShouldExclude(ad);

  // Assert
  EXPECT_FALSE(should_exclude);
}

TEST_F(BatAdsDayPartingFrequencyCapTest, AllowIfRightDayAndHours) {
  // Arrange
  CreativeAdInfo ad;
  ad.creative_set_id = kCreativeSetId;

  base::Time::Exploded exploded;
  base::Time::Now().LocalExplode(&exploded);
  int current_time = 60 * exploded.hour + exploded.minute;

  ad.day_parts =
      base::NumberToString(exploded.day_of_week) + "_" +
      base::NumberToString(current_time - base::Time::kMinutesPerHour) + "_" +
      base::NumberToString(current_time + base::Time::kMinutesPerHour);
  // Act
  const bool should_exclude = frequency_cap_->ShouldExclude(ad);

  // Assert
  EXPECT_FALSE(should_exclude);
}

TEST_F(BatAdsDayPartingFrequencyCapTest, AllowForMultipleDays) {
  // Arrange
  CreativeAdInfo ad;
  ad.creative_set_id = kCreativeSetId;

  base::Time::Exploded exploded;
  base::Time::Now().LocalExplode(&exploded);
  int current_time = 60 * exploded.hour + exploded.minute;

  ad.day_parts =
      std::string("0123456") + "_" +
      base::NumberToString(current_time - base::Time::kMinutesPerHour) + "_" +
      base::NumberToString(current_time + base::Time::kMinutesPerHour);
  // Act
  const bool should_exclude = frequency_cap_->ShouldExclude(ad);

  // Assert
  EXPECT_FALSE(should_exclude);
}

TEST_F(BatAdsDayPartingFrequencyCapTest, AllowIfOneMatchExists) {
  // Arrange
  CreativeAdInfo ad;
  ad.creative_set_id = kCreativeSetId;

  base::Time::Exploded exploded;
  base::Time::Now().LocalExplode(&exploded);
  int current_time = 60 * exploded.hour + exploded.minute;
  std::string tomorrow_dow = base::NumberToString((exploded.day_of_week + 1) % 7);
  std::string current_dow = base::NumberToString(exploded.day_of_week);

  ad.day_parts =
      tomorrow_dow + "_" +
      base::NumberToString(current_time - 60) + "_" +
      base::NumberToString(current_time + 60) + ",";

  ad.day_parts +=
      tomorrow_dow + "_" +
      base::NumberToString(current_time + 120) + "_" +
      base::NumberToString(current_time + 180) + ",";

  ad.day_parts +=
      current_dow + "_" +
      base::NumberToString(current_time - 60) + "_" +
      base::NumberToString(current_time + 60) + ",";
  // Act
  const bool should_exclude = frequency_cap_->ShouldExclude(ad);

  // Assert
  EXPECT_FALSE(should_exclude);
}

TEST_F(BatAdsDayPartingFrequencyCapTest, DisallowIfNoMatches) {
  // Arrange
  CreativeAdInfo ad;
  ad.creative_set_id = kCreativeSetId;

  base::Time::Exploded exploded;
  base::Time::Now().LocalExplode(&exploded);
  int current_time = 60 * exploded.hour + exploded.minute;
  std::string tomorrow_dow = base::NumberToString((exploded.day_of_week + 1) % 7);
  std::string current_dow = base::NumberToString(exploded.day_of_week);

  ad.day_parts =
      tomorrow_dow + "_" +
      base::NumberToString(current_time - 60) + "_" +
      base::NumberToString(current_time + 60) + ",";

  ad.day_parts +=
      tomorrow_dow + "_" +
      base::NumberToString(current_time + 120) + "_" +
      base::NumberToString(current_time + 180) + ",";

  ad.day_parts +=
      current_dow + "_" +
      base::NumberToString(current_time + 60) + "_" +
      base::NumberToString(current_time + 120) + ",";

  // Act
  const bool should_exclude = frequency_cap_->ShouldExclude(ad);

  // Assert
  EXPECT_TRUE(should_exclude);
}


TEST_F(BatAdsDayPartingFrequencyCapTest, DisallowIfWrongDay) {
  // Arrange
  CreativeAdInfo ad;
  ad.creative_set_id = kCreativeSetId;

  base::Time::Exploded exploded;
  base::Time::Now().LocalExplode(&exploded);
  int current_time = 60 * exploded.hour + exploded.minute;

  // Go to next day
  std::string tomorrow_dow = base::NumberToString((exploded.day_of_week + 1) % 7);

  ad.day_parts =
      tomorrow_dow + "_" +
      base::NumberToString(current_time - 60) + "_" +
      base::NumberToString(current_time + 60) + ",";
  // Act
  const bool should_exclude = frequency_cap_->ShouldExclude(ad);

  // Assert
  EXPECT_TRUE(should_exclude);
}

TEST_F(BatAdsDayPartingFrequencyCapTest, DisallowIfWrongHours) {
  // Arrange
  CreativeAdInfo ad;
  ad.creative_set_id = kCreativeSetId;

  base::Time::Exploded exploded;
  base::Time::Now().LocalExplode(&exploded);
  int current_time = 60 * exploded.hour + exploded.minute;

  // Go to next day
  std::string tomorrow_dow = base::NumberToString((exploded.day_of_week + 1) % 7);

  ad.day_parts =
      tomorrow_dow + "_" +
      base::NumberToString(current_time - 60) + "_" +
      base::NumberToString(current_time + 60) + ",";
  // Act
  const bool should_exclude = frequency_cap_->ShouldExclude(ad);

  // Assert
  EXPECT_TRUE(should_exclude);
}

}  // namespace ads
