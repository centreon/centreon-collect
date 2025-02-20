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

#include "check_event_log_data.hh"

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

  mock_event_data data;
  data.event_id = 5;
  EXPECT_TRUE(filter1.allow(data));
  EXPECT_FALSE(filter2.allow(data));
  EXPECT_TRUE(filter3.allow(data));
  EXPECT_TRUE(filter4.allow(data));
  EXPECT_TRUE(filter5.allow(data));

  data.event_id = 6;
  EXPECT_FALSE(filter1.allow(data));
  EXPECT_TRUE(filter2.allow(data));
  EXPECT_TRUE(filter3.allow(data));
  EXPECT_TRUE(filter4.allow(data));
  EXPECT_TRUE(filter5.allow(data));

  data.event_id = 7;
  EXPECT_FALSE(filter1.allow(data));
  EXPECT_TRUE(filter2.allow(data));
  EXPECT_FALSE(filter3.allow(data));
  EXPECT_FALSE(filter4.allow(data));
  EXPECT_TRUE(filter5.allow(data));

  data.event_id = 8;
  EXPECT_FALSE(filter1.allow(data));
  EXPECT_TRUE(filter2.allow(data));
  EXPECT_FALSE(filter3.allow(data));
  EXPECT_FALSE(filter4.allow(data));
  EXPECT_FALSE(filter5.allow(data));
}

TEST(eventlog_filter, level) {
  event_filter filter1("level=5", spdlog::default_logger());
  event_filter filter2("level!='debug'", spdlog::default_logger());
  event_filter filter3("level in (debug, 6)", spdlog::default_logger());
  event_filter filter4("level='debug' or level=6", spdlog::default_logger());
  event_filter filter5("level in (5, 6) or level in ('info')",
                       spdlog::default_logger());

  mock_event_data data;
  data.level = 5;
  EXPECT_TRUE(filter1.allow(data));
  EXPECT_FALSE(filter2.allow(data));
  EXPECT_TRUE(filter3.allow(data));
  EXPECT_TRUE(filter4.allow(data));
  EXPECT_TRUE(filter5.allow(data));

  data.level = 6;
  EXPECT_FALSE(filter1.allow(data));
  EXPECT_TRUE(filter2.allow(data));
  EXPECT_TRUE(filter3.allow(data));
  EXPECT_TRUE(filter4.allow(data));
  EXPECT_TRUE(filter5.allow(data));

  data.level = 4;  // info
  EXPECT_FALSE(filter1.allow(data));
  EXPECT_TRUE(filter2.allow(data));
  EXPECT_FALSE(filter3.allow(data));
  EXPECT_FALSE(filter4.allow(data));
  EXPECT_TRUE(filter5.allow(data));

  data.level = 8;  // info
  EXPECT_FALSE(filter1.allow(data));
  EXPECT_TRUE(filter2.allow(data));
  EXPECT_FALSE(filter3.allow(data));
  EXPECT_FALSE(filter4.allow(data));
  EXPECT_FALSE(filter5.allow(data));
}

TEST(eventlog_filter, keywords) {
  event_filter filter1("keywords='auditsuccess'", spdlog::default_logger());
  event_filter filter2("keywords!='auditsuccess'", spdlog::default_logger());
  event_filter filter3("keywords in (auditfailure)", spdlog::default_logger());
  event_filter filter4("keywords in (auditsuccess, auditfailure)",
                       spdlog::default_logger());
  event_filter filter5(
      "keywords in (auditfailure) or keywords notin ('auditsuccess')",
      spdlog::default_logger());

  mock_event_data data;
  data.keywords = 0;
  EXPECT_FALSE(filter1.allow(data));
  EXPECT_TRUE(filter2.allow(data));
  EXPECT_FALSE(filter3.allow(data));
  EXPECT_FALSE(filter4.allow(data));
  EXPECT_TRUE(filter5.allow(data));

  data.keywords = 0x8020000000000000L;  // auditsuccess
  EXPECT_TRUE(filter1.allow(data));
  EXPECT_FALSE(filter2.allow(data));
  EXPECT_FALSE(filter3.allow(data));
  EXPECT_TRUE(filter4.allow(data));
  EXPECT_FALSE(filter5.allow(data));

  data.keywords = 0x8010000000000000L;  // auditfailure
  EXPECT_FALSE(filter1.allow(data));
  EXPECT_TRUE(filter2.allow(data));
  EXPECT_TRUE(filter3.allow(data));
  EXPECT_TRUE(filter4.allow(data));
  EXPECT_TRUE(filter5.allow(data));

  data.keywords = 0x8030000000000000L;  // auditfailure & auditsuccess
  EXPECT_FALSE(filter1.allow(data));
  EXPECT_TRUE(filter2.allow(data));
  EXPECT_TRUE(filter3.allow(data));
  EXPECT_TRUE(filter4.allow(data));
  EXPECT_TRUE(filter5.allow(data));
}

TEST(eventlog_filter, computer) {
  event_filter filter1("computer == 'computer1'", spdlog::default_logger());
  event_filter filter2("computer != 'computer1'", spdlog::default_logger());
  event_filter filter3("computer in ('computer1', 'computer2')",
                       spdlog::default_logger());
  event_filter filter4("computer == 'computer1' or computer == 'computer2'",
                       spdlog::default_logger());
  event_filter filter5(
      "computer in ('computer1', 'computer2') or computer in ( computer3 )",
      spdlog::default_logger());

  mock_event_data data;

  data.computer = L"computer1";
  EXPECT_TRUE(filter1.allow(data));
  EXPECT_FALSE(filter2.allow(data));
  EXPECT_TRUE(filter3.allow(data));
  EXPECT_TRUE(filter4.allow(data));
  EXPECT_TRUE(filter5.allow(data));

  data.computer = L"computer2";
  EXPECT_FALSE(filter1.allow(data));
  EXPECT_TRUE(filter2.allow(data));
  EXPECT_TRUE(filter3.allow(data));
  EXPECT_TRUE(filter4.allow(data));
  EXPECT_TRUE(filter5.allow(data));

  data.computer = L"computer3";
  EXPECT_FALSE(filter1.allow(data));
  EXPECT_TRUE(filter2.allow(data));
  EXPECT_FALSE(filter3.allow(data));
  EXPECT_FALSE(filter4.allow(data));
  EXPECT_TRUE(filter5.allow(data));

  data.computer = L"computer4";
  EXPECT_FALSE(filter1.allow(data));
  EXPECT_TRUE(filter2.allow(data));
  EXPECT_FALSE(filter3.allow(data));
  EXPECT_FALSE(filter4.allow(data));
  EXPECT_FALSE(filter5.allow(data));
}

TEST(eventlog_filter, channel) {
  event_filter filter1("channel == 'channel1'", spdlog::default_logger());
  event_filter filter2("channel != 'channel1'", spdlog::default_logger());
  event_filter filter3("channel in ('channel1', 'channel2')",
                       spdlog::default_logger());
  event_filter filter4("channel == 'channel1' or channel == 'channel2'",
                       spdlog::default_logger());
  event_filter filter5(
      "channel in ('channel1', 'channel2') or channel in ( channel3 )",
      spdlog::default_logger());

  mock_event_data data;

  data.channel = L"channel1";
  EXPECT_TRUE(filter1.allow(data));
  EXPECT_FALSE(filter2.allow(data));
  EXPECT_TRUE(filter3.allow(data));
  EXPECT_TRUE(filter4.allow(data));
  EXPECT_TRUE(filter5.allow(data));

  data.channel = L"channel2";
  EXPECT_FALSE(filter1.allow(data));
  EXPECT_TRUE(filter2.allow(data));
  EXPECT_TRUE(filter3.allow(data));
  EXPECT_TRUE(filter4.allow(data));
  EXPECT_TRUE(filter5.allow(data));

  data.channel = L"channel3";
  EXPECT_FALSE(filter1.allow(data));
  EXPECT_TRUE(filter2.allow(data));
  EXPECT_FALSE(filter3.allow(data));
  EXPECT_FALSE(filter4.allow(data));
  EXPECT_TRUE(filter5.allow(data));

  data.channel = L"channel4";
  EXPECT_FALSE(filter1.allow(data));
  EXPECT_TRUE(filter2.allow(data));
  EXPECT_FALSE(filter3.allow(data));
  EXPECT_FALSE(filter4.allow(data));
  EXPECT_FALSE(filter5.allow(data));
}

TEST(eventlog_filter, composite) {
  event_filter filter1("provider == 'provider1' and event_id=5",
                       spdlog::default_logger());
  event_filter filter2("provider == 'provider1' and event_id=6",
                       spdlog::default_logger());
  event_filter filter3("provider == 'provider1' and event_id=5 or event_id=6",
                       spdlog::default_logger());
  event_filter filter4(
      "provider == 'provider1' and event_id=5 or event_id=6 and level=5",
      spdlog::default_logger());
  event_filter filter5(
      "provider == 'provider1' and event_id=5 or event_id=6 and level=6",
      spdlog::default_logger());
  event_filter filter6(
      "provider == 'provider1' and event_id=5 or event_id=6 and level=6 or "
      "level=5",
      spdlog::default_logger());
  event_filter filter7(
      "provider == 'provider1' and event_id=5 or event_id=6 and level=6 or "
      "level=5 and keywords='auditsuccess'",
      spdlog::default_logger());
  event_filter filter8(
      "provider == 'provider1' and event_id=5 or event_id=6 and level=6 or "
      "level=5 and keywords='auditfailure'",
      spdlog::default_logger());
  event_filter filter9(
      "provider == 'provider1' and event_id=5 or event_id=6 and level=6 or "
      "level=5 and keywords='auditfailure' or keywords='auditsuccess'",
      spdlog::default_logger());
  event_filter filter10(
      "provider == 'provider1' and event_id=5 or event_id=6 and level=6 or "
      "level=5 and keywords='auditfailure' or keywords='auditsuccess' and "
      "computer='computer1'",
      spdlog::default_logger());
  event_filter filter11(
      "provider == 'provider1' and event_id=5 or event_id=6 and level=6 or "
      "level=5 and keywords='auditfailure' or keywords='auditsuccess' and "
      "computer='computer2'",
      spdlog::default_logger());
  event_filter filter12(
      "provider == 'provider1' and event_id=5 or event_id=6 and level=6 or "
      "level=5 and keywords='auditfailure' or keywords='auditsuccess' and "
      "computer='computer2' or computer='computer1'",
      spdlog::default_logger());
  event_filter filter13(
      "provider == 'provider1' and event_id=5 or event_id=6 and level=6 or "
      "level=5 and keywords='auditfailure' or keywords='auditsuccess' and "
      "computer='computer2' or computer='computer1' and channel='channel1'",
      spdlog::default_logger());

  mock_event_data data;
  data.provider = L"provider1";
  data.event_id = 5;
  data.level = 5;
  data.keywords = 0x8020000000000000L;  // auditsuccess
  data.computer = L"computer1";
  data.channel = L"channel1";
  EXPECT_TRUE(filter1.allow(data));
  EXPECT_FALSE(filter2.allow(data));
  EXPECT_TRUE(filter3.allow(data));
  EXPECT_TRUE(filter4.allow(data));
  EXPECT_TRUE(filter5.allow(data));
  EXPECT_TRUE(filter6.allow(data));
  EXPECT_TRUE(filter7.allow(data));
  EXPECT_TRUE(filter8.allow(data));
  EXPECT_TRUE(filter9.allow(data));
  EXPECT_TRUE(filter10.allow(data));
  EXPECT_TRUE(filter11.allow(data));
  EXPECT_TRUE(filter12.allow(data));
  EXPECT_TRUE(filter13.allow(data));

  data.provider = L"provider1";
  data.event_id = 6;
  data.level = 6;
  data.keywords = 0x8010000000000000L;  // auditfailure
  data.computer = L"computer2";
  data.channel = L"channel2";
  EXPECT_FALSE(filter1.allow(data));
  EXPECT_TRUE(filter2.allow(data));
  EXPECT_TRUE(filter3.allow(data));
  EXPECT_FALSE(filter4.allow(data));
  EXPECT_TRUE(filter5.allow(data));
  EXPECT_TRUE(filter6.allow(data));
  EXPECT_TRUE(filter7.allow(data));
  EXPECT_TRUE(filter8.allow(data));
  EXPECT_TRUE(filter9.allow(data));
  EXPECT_TRUE(filter10.allow(data));
  EXPECT_TRUE(filter11.allow(data));
  EXPECT_TRUE(filter12.allow(data));
  EXPECT_TRUE(filter13.allow(data));

  data.provider = L"provider2";
  data.event_id = 5;
  data.level = 5;
  data.keywords = 0x8020000000000000L;  // auditsuccess
  data.computer = L"computer1";
  data.channel = L"channel1";
  EXPECT_FALSE(filter1.allow(data));
  EXPECT_FALSE(filter2.allow(data));
  EXPECT_FALSE(filter3.allow(data));
  EXPECT_FALSE(filter4.allow(data));
  EXPECT_FALSE(filter5.allow(data));
  EXPECT_TRUE(filter6.allow(data));
  EXPECT_TRUE(filter7.allow(data));
  EXPECT_FALSE(filter8.allow(data));
  EXPECT_TRUE(filter9.allow(data));
  EXPECT_TRUE(filter10.allow(data));
  EXPECT_FALSE(filter11.allow(data));
  EXPECT_TRUE(filter12.allow(data));
  EXPECT_TRUE(filter13.allow(data));

  data.provider = L"provider1";
  data.event_id = 7;
  data.level = 7;
  data.keywords = 0x8030000000000000L;  // auditfailure & auditsuccess
  data.computer = L"computer3";
  data.channel = L"channel3";
  EXPECT_FALSE(filter1.allow(data));
  EXPECT_FALSE(filter2.allow(data));
  EXPECT_FALSE(filter3.allow(data));
  EXPECT_FALSE(filter4.allow(data));
  EXPECT_FALSE(filter5.allow(data));
  EXPECT_FALSE(filter6.allow(data));
  EXPECT_FALSE(filter7.allow(data));
  EXPECT_FALSE(filter8.allow(data));
  EXPECT_FALSE(filter9.allow(data));
  EXPECT_FALSE(filter10.allow(data));
  EXPECT_FALSE(filter11.allow(data));
  EXPECT_FALSE(filter12.allow(data));
  EXPECT_FALSE(filter13.allow(data));
}
