/**
 * Copyright 2025 Centreon
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

#include "check_event_log_data_test.hh"
#include "event_log/uniq.hh"

using namespace com::centreon::agent;
using namespace com::centreon::agent::event_log;

using event_cont =
    absl::flat_hash_set<const event*, event_comparator, event_comparator>;

/**
 * @brief Given the default event log unique string, we inject some mocked
 * events and check the container size
 */
TEST(eventlog_uniq, default_uniq) {
  event_comparator uniq("${log}-${source}-${id}", spdlog::default_logger());

  mock_event_data evt_data;
  evt_data.provider = L"src1";
  evt_data.event_id = 455;

  event_cont cont(0, uniq, uniq);
  using namespace std::string_literals;

  event evt1(evt_data, {}, ""s);
  cont.insert(&evt1);
  EXPECT_EQ(cont.size(), 1);

  evt_data.channel = L"channel1";
  event evt2(evt_data, {}, ""s);
  cont.insert(&evt2);
  EXPECT_EQ(cont.size(), 1);

  evt_data.event_id = 456;
  event evt3(evt_data, {}, ""s);
  cont.insert(&evt3);
  EXPECT_EQ(cont.size(), 2);

  evt_data.provider = L"src2";
  event evt4(evt_data, {}, ""s);
  cont.insert(&evt4);
  EXPECT_EQ(cont.size(), 3);
}

/**
 * @brief Given an empty unique string, we inject some mocked events none have
 * to been dropped
 *
 */
TEST(eventlog_uniq, empty) {
  event_comparator uniq("", spdlog::default_logger());

  mock_event_data evt_data;
  evt_data.provider = L"src1";
  evt_data.event_id = 455;

  event_cont cont(0, uniq, uniq);
  using namespace std::string_literals;

  event evt1(evt_data, {}, ""s);
  cont.insert(&evt1);
  EXPECT_EQ(cont.size(), 1);

  evt_data.channel = L"channel1";
  event evt2(evt_data, {}, ""s);
  cont.insert(&evt2);
  EXPECT_EQ(cont.size(), 2);

  evt_data.record_id = 456;
  event evt3(evt_data, {}, ""s);
  cont.insert(&evt3);
  EXPECT_EQ(cont.size(), 3);

  evt_data.provider = L"src2";
  event evt4(evt_data, {}, ""s);
  cont.insert(&evt4);
  EXPECT_EQ(cont.size(), 4);

  evt_data.time_created += 1000;
  event evt5(evt_data, {}, ""s);
  cont.insert(&evt5);
  EXPECT_EQ(cont.size(), 5);
}

/**
 * @brief Given a unique string with only channel and message, we inject some
 * mocked events and check the container size according to grouping by message
 * and channel
 */
TEST(eventlog_uniq, channel_message) {
  event_comparator uniq("${channel}-${message}", spdlog::default_logger());

  mock_event_data evt_data;
  evt_data.channel = L"channel1";
  evt_data.event_id = 455;

  event_cont cont(0, uniq, uniq);
  using namespace std::string_literals;

  event evt1(evt_data, {}, ""s);
  cont.insert(&evt1);
  EXPECT_EQ(cont.size(), 1);

  evt_data.event_id = 456;
  event evt2(evt_data, {}, ""s);
  cont.insert(&evt2);
  EXPECT_EQ(cont.size(), 1);

  evt_data.channel = L"channel2";
  event evt3(evt_data, {}, ""s);
  cont.insert(&evt3);
  EXPECT_EQ(cont.size(), 2);

  event evt4(evt_data, {}, "mess1"s);
  cont.insert(&evt4);
  EXPECT_EQ(cont.size(), 3);

  evt_data.provider = L"provider1";
  event evt5(evt_data, {}, "mess1"s);
  cont.insert(&evt5);
  EXPECT_EQ(cont.size(), 3);
}