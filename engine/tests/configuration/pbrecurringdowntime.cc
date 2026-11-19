/**
 * Copyright 2026 Centreon (https://www.centreon.com/)
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * For more information : contact@centreon.com
 *
 */

#include <gtest/gtest.h>
#include <fstream>

#include "common/engine_conf/parser.hh"
#include "common/engine_conf/recurringdowntime_helper.hh"
#include "helper.hh"

using namespace com::centreon::engine;
using namespace com::centreon::engine::configuration;

class recurringdowntime_config_test : public ::testing::Test {
 public:
  void SetUp() override { init_config_state(); }
  void TearDown() override { deinit_config_state(); }
};

TEST_F(recurringdowntime_config_test, weekly_fixed_period) {
  RecurringdowntimePeriod p;
  ASSERT_TRUE(recurringdowntime_helper::parse_period(
      "weekly:1,2,3,4,5 22:00-23:30 fixed", &p));
  EXPECT_EQ(p.kind(), RecurringdowntimePeriod::weekly);
  ASSERT_EQ(p.days_size(), 5);
  EXPECT_EQ(p.days(0), 1u);
  EXPECT_EQ(p.days(4), 5u);
  EXPECT_EQ(p.start_minute(), 22u * 60);
  EXPECT_EQ(p.end_minute(), 23u * 60 + 30);
  EXPECT_TRUE(p.fixed());
  EXPECT_EQ(p.duration(), 0u);
}

TEST_F(recurringdowntime_config_test, monthly_flexible_period) {
  RecurringdowntimePeriod p;
  ASSERT_TRUE(recurringdowntime_helper::parse_period(
      "monthly:1,15,31 02:00-04:00 flexible:3600", &p));
  EXPECT_EQ(p.kind(), RecurringdowntimePeriod::monthly);
  ASSERT_EQ(p.days_size(), 3);
  EXPECT_EQ(p.days(2), 31u);
  EXPECT_EQ(p.start_minute(), 120u);
  EXPECT_EQ(p.end_minute(), 240u);
  EXPECT_FALSE(p.fixed());
  EXPECT_EQ(p.duration(), 3600u);
}

TEST_F(recurringdowntime_config_test, nth_period_with_end_of_day) {
  RecurringdowntimePeriod p;
  ASSERT_TRUE(recurringdowntime_helper::parse_period(
      "nth:last:7 01:00-24:00 fixed", &p));
  EXPECT_EQ(p.kind(), RecurringdowntimePeriod::nth);
  EXPECT_EQ(p.rank(), -1);
  ASSERT_EQ(p.days_size(), 1);
  EXPECT_EQ(p.days(0), 7u);
  EXPECT_EQ(p.start_minute(), 60u);
  EXPECT_EQ(p.end_minute(), 1440u);

  const std::pair<const char*, int> ranks[] = {
      {"first", 1}, {"second", 2}, {"third", 3}, {"fourth", 4}};
  for (const auto& [word, rank] : ranks) {
    RecurringdowntimePeriod q;
    ASSERT_TRUE(recurringdowntime_helper::parse_period(
        fmt::format("nth:{}:2 00:00-01:00 fixed", word), &q))
        << word;
    EXPECT_EQ(q.rank(), rank) << word;
  }
}

TEST_F(recurringdowntime_config_test, flexible_without_duration_accepted) {
  RecurringdowntimePeriod p;
  ASSERT_TRUE(recurringdowntime_helper::parse_period(
      "weekly:6 10:00-11:00 flexible:0", &p));
  EXPECT_FALSE(p.fixed());
  EXPECT_EQ(p.duration(), 0u);
}

TEST_F(recurringdowntime_config_test, invalid_periods_rejected) {
  const char* invalid[] = {
      "",
      "weekly:1 01:00-02:00",
      "weekly:1 01:00-02:00 fixed extra",
      "daily:1 01:00-02:00 fixed",
      "weekly: 01:00-02:00 fixed",
      "weekly:0 01:00-02:00 fixed",
      "weekly:8 01:00-02:00 fixed",
      "weekly:1,x 01:00-02:00 fixed",
      "monthly:0 01:00-02:00 fixed",
      "monthly:32 01:00-02:00 fixed",
      "nth:fifth:1 01:00-02:00 fixed",
      "nth:last:1,2 01:00-02:00 fixed",
      "nth:last 01:00-02:00 fixed",
      "weekly:1 02:00-01:00 fixed",
      "weekly:1 02:00-02:00 fixed",
      "weekly:1 24:00-24:00 fixed",
      "weekly:1 01:00-24:01 fixed",
      "weekly:1 01:60-02:00 fixed",
      "weekly:1 1:5-02:00 fixed",
      "weekly:1 01:00 fixed",
      "weekly:1 01:00-02:00 flexible",
      "weekly:1 01:00-02:00 flexible:-1",
      "weekly:1 01:00-02:00 flexible:abc",
      "weekly:1 01:00-02:00 sometimes",
  };
  for (const char* value : invalid) {
    RecurringdowntimePeriod p;
    EXPECT_FALSE(recurringdowntime_helper::parse_period(value, &p))
        << "'" << value << "' should be refused";
  }
}

TEST_F(recurringdowntime_config_test, targets_parsed) {
  Recurringdowntime rd;
  recurringdowntime_helper hlp(&rd);
  ASSERT_TRUE(hlp.hook("host_ids", "14, 15"));
  ASSERT_TRUE(hlp.hook("service_ids", "14:26,15:30"));
  ASSERT_EQ(rd.host_ids_size(), 2);
  EXPECT_EQ(rd.host_ids(1), 15u);
  ASSERT_EQ(rd.service_ids_size(), 2);
  EXPECT_EQ(rd.service_ids(1).host_id(), 15u);
  EXPECT_EQ(rd.service_ids(1).service_id(), 30u);

  Recurringdowntime bad;
  recurringdowntime_helper bad_hlp(&bad);
  EXPECT_FALSE(bad_hlp.hook("host_ids", "14,abc"));
  EXPECT_FALSE(bad_hlp.hook("host_ids", "0"));
  EXPECT_FALSE(bad_hlp.hook("service_ids", "14"));
  EXPECT_FALSE(bad_hlp.hook("service_ids", "14:"));
  EXPECT_FALSE(bad_hlp.hook("service_ids", "14:0"));
}

TEST_F(recurringdowntime_config_test, missing_id_period_or_target_invalid) {
  error_cnt err;
  Recurringdowntime rd;
  recurringdowntime_helper hlp(&rd);
  rd.set_downtime_id(3);
  ASSERT_TRUE(hlp.hook("host_ids", "14"));
  EXPECT_THROW(hlp.check_validity(err), std::exception);  // no period

  ASSERT_TRUE(hlp.hook("period", "weekly:1 01:00-02:00 fixed"));
  EXPECT_NO_THROW(hlp.check_validity(err));

  rd.clear_host_ids();
  EXPECT_THROW(hlp.check_validity(err), std::exception);  // no target

  ASSERT_TRUE(hlp.hook("service_ids", "14:26"));
  EXPECT_NO_THROW(hlp.check_validity(err));

  rd.set_downtime_id(0);
  EXPECT_THROW(hlp.check_validity(err), std::exception);  // no id
  EXPECT_EQ(err.config_errors, 3u);
}

class recurringdowntime_parser_test : public recurringdowntime_config_test {
 protected:
  const std::string _main =
      fmt::format("/tmp/recurringdowntime_test_{}.cfg", getpid());
  const std::string _objects =
      fmt::format("/tmp/recurringdowntimes_{}.cfg", getpid());

  void _write(const std::string& objects) {
    std::ofstream(_main) << "cfg_file=" << _objects << "\n";
    std::ofstream(_objects) << objects;
  }

 public:
  void TearDown() override {
    std::remove(_main.c_str());
    std::remove(_objects.c_str());
    recurringdowntime_config_test::TearDown();
  }
};

// The object as the Centreon Web generator writes it.
TEST_F(recurringdowntime_parser_test, web_sample_parsed) {
  _write(
      "define recurringdowntime {\n"
      "    downtime_id                    3\n"
      "    downtime_name                  Weekly maintenance\n"
      "    timezone                       :Europe/Paris\n"
      "    host_ids                       14,15\n"
      "    service_ids                    14:26,15:30\n"
      "    period                         weekly:1,2,3,4,5 22:00-23:30 fixed\n"
      "    period                         monthly:1,15 02:00-04:00 "
      "flexible:3600\n"
      "    period                         nth:last:7 01:00-24:00 fixed\n"
      "}\n"
      "define recurringdowntime {\n"
      "    downtime_id                    4\n"
      "    service_ids                    14:26\n"
      "    period                         weekly:6 10:00-11:00 flexible:0\n"
      "}\n");
  State cfg;
  parser p;
  error_cnt err;
  ASSERT_NO_THROW(p.parse(_main, &cfg, err));
  ASSERT_EQ(cfg.recurringdowntimes_size(), 2);

  const Recurringdowntime& rd = cfg.recurringdowntimes(0);
  EXPECT_EQ(rd.downtime_id(), 3u);
  EXPECT_EQ(rd.downtime_name(), "Weekly maintenance");
  EXPECT_EQ(rd.timezone(), ":Europe/Paris");
  ASSERT_EQ(rd.host_ids_size(), 2);
  EXPECT_EQ(rd.host_ids(0), 14u);
  ASSERT_EQ(rd.service_ids_size(), 2);
  EXPECT_EQ(rd.service_ids(0).service_id(), 26u);
  ASSERT_EQ(rd.periods_size(), 3);
  EXPECT_EQ(rd.periods(1).kind(), RecurringdowntimePeriod::monthly);
  EXPECT_EQ(rd.periods(2).end_minute(), 1440u);

  const Recurringdowntime& rd4 = cfg.recurringdowntimes(1);
  EXPECT_EQ(rd4.downtime_id(), 4u);
  EXPECT_FALSE(rd4.has_timezone());
  EXPECT_EQ(rd4.host_ids_size(), 0);
  EXPECT_EQ(rd4.periods(0).duration(), 0u);
}

TEST_F(recurringdowntime_parser_test, invalid_period_is_a_configuration_error) {
  _write(
      "define recurringdowntime {\n"
      "    downtime_id 3\n"
      "    host_ids    14\n"
      "    period      weekly:1 23:00-22:00 fixed\n"
      "}\n");
  State cfg;
  parser p;
  error_cnt err;
  EXPECT_THROW(p.parse(_main, &cfg, err), std::exception);
}

TEST_F(recurringdowntime_parser_test, missing_target_is_a_configuration_error) {
  _write(
      "define recurringdowntime {\n"
      "    downtime_id 3\n"
      "    period      weekly:1 22:00-23:00 fixed\n"
      "}\n");
  State cfg;
  parser p;
  error_cnt err;
  EXPECT_THROW(p.parse(_main, &cfg, err), std::exception);
  EXPECT_EQ(err.config_errors, 1u);
}
