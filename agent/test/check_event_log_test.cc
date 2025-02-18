/**
 * Copyright 2024 Centreon
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * For more information : contact@centreon.com
 */

#include <gtest/gtest.h>
#include <spdlog/spdlog.h>

#include "check_event_log.hh"

using namespace com::centreon::agent;
using namespace com::centreon::agent::check_event_log_detail;

struct mock_event_data : public event_data {
  std::wstring provider;
  uint16_t event_id;
  uint8_t level;
  uint16_t task;
  int64_t keywords;
  uint64_t time_created;
  uint64_t record_id;
  std::wstring computer;
  std::wstring channel;

  mock_event_data() {}

  std::wstring_view get_provider() const override { return provider; }

  uint16_t get_event_id() const override { return event_id; }

  uint8_t get_level() const override { return level; }

  uint16_t get_task() const override { return task; }

  int64_t get_keywords() const override { return keywords; }

  uint64_t get_time_created() const override { return time_created; }

  uint64_t get_record_id() const override { return record_id; }

  std::wstring_view get_computer() const override { return computer; }

  std::wstring_view get_channel() const override { return channel; }
};

TEST(eventlog_filter, provider) {
  event_filter filter1("provider == 'provider1'", spdlog::default_logger());
  event_filter filter2("provider != 'provider1'", spdlog::default_logger());
  event_filter filter3("provider in ('provider1', 'provider2')",
                       spdlog::default_logger());
  event_filter filter4("provider == 'provider1' or provider == 'provider2'",
                       spdlog::default_logger());
  event_filter filter5(
      "provider in ('provider1', 'provider2') or provider in ( provider3 )",
      spdlog::default_logger());

  mock_event_data data;

  data.provider = L"provider1";
  EXPECT_TRUE(filter1.allow(data));
  EXPECT_FALSE(filter2.allow(data));
  EXPECT_TRUE(filter3.allow(data));
  EXPECT_TRUE(filter4.allow(data));
  EXPECT_TRUE(filter5.allow(data));

  data.provider = L"provider2";
  EXPECT_FALSE(filter1.allow(data));
  EXPECT_TRUE(filter2.allow(data));
  EXPECT_TRUE(filter3.allow(data));
  EXPECT_TRUE(filter4.allow(data));
  EXPECT_TRUE(filter5.allow(data));

  data.provider = L"provider3";
  EXPECT_FALSE(filter1.allow(data));
  EXPECT_TRUE(filter2.allow(data));
  EXPECT_FALSE(filter3.allow(data));
  EXPECT_FALSE(filter4.allow(data));
  EXPECT_TRUE(filter5.allow(data));

  data.provider = L"provider4";
  EXPECT_FALSE(filter1.allow(data));
  EXPECT_TRUE(filter2.allow(data));
  EXPECT_FALSE(filter3.allow(data));
  EXPECT_FALSE(filter4.allow(data));
  EXPECT_FALSE(filter5.allow(data));
}

TEST(eventlog_filter, event_id) {
  event_filter filter1("event_id=5", spdlog::default_logger());
  event_filter filter2("event_id!=5", spdlog::default_logger());
  event_filter filter3("event_id in (5, 6)", spdlog::default_logger());
  event_filter filter4("event_id=5 or event_id=6", spdlog::default_logger());
  event_filter filter5("event_id in (5, 6) or event_id in (7)",
                       spdlog::default_logger());
}
