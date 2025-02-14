/**
 * Copyright 2022 Centreon
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
#include "../../timeperiod/utils.hh"
#include "com/centreon/engine/configuration/applier/timeperiod.hh"
#include "com/centreon/engine/exceptions/error.hh"
#include "com/centreon/engine/string.hh"
#include "com/centreon/engine/timeperiod.hh"
#include "common/engine_conf/timeperiod_helper.hh"
#include "gtest/gtest.h"

#include "helper.hh"

using namespace com::centreon::engine;

struct test_param {
  std::string name;
  std::string now;       // YYYY-MM-DD HH:MM:SS format
  std::string prefered;  // YYYY-MM-DD HH:MM:SS format
  std::string expected;  // YYYY-MM-DD HH:MM:SS format
};

std::string PrintToString(const test_param& data) {
  std::stringstream ss;
  ss << "name: " << data.name << " ; now: " << data.now
     << " ; prefered: " << data.prefered << " ; expected: " << data.expected;
  return ss.str();
}

class timeperiod_exception : public ::testing::TestWithParam<test_param> {
 protected:
  static configuration::applier::timeperiod _applier;
  static void SetUpTestSuite() {
    init_config_state();
    com::centreon::engine::timeperiod::timeperiods.clear();
    parse_timeperiods_cfg_file("tests/timeperiods.cfg");
  }

  static void TearDownTestSuite() {
    com::centreon::engine::timeperiod::timeperiods.clear();
  }

  static void parse_timeperiods_cfg_file(const std::string& file_path);
};

configuration::applier::timeperiod timeperiod_exception::_applier;

time_t gmt_strtotimet(std::string const& str) {
  tm t;
  memset(&t, 0, sizeof(t));
  if (!strptime(str.c_str(), "%Y-%m-%d %H:%M:%S", &t))
    throw(engine_error() << "invalid date format");
  t.tm_isdst = 0;
  return (timegm(&t));
}

char* ctime_gmt(time_t t) {
  return asctime(gmtime(&t));
}

void timeperiod_exception::parse_timeperiods_cfg_file(
    const std::string& file_path) {
  std::ifstream f(file_path);
  std::string line;

  bool wait_time_period_begin = true;

  std::unique_ptr<configuration::Timeperiod> conf(
      std::make_unique<configuration::Timeperiod>());
  std::unique_ptr<configuration::timeperiod_helper> conf_hlp =
      std::make_unique<configuration::timeperiod_helper>(conf.get());
  while (!f.eof()) {
    std::getline(f, line);

    if (line.empty()) {
      continue;
    }

    if (wait_time_period_begin) {
      wait_time_period_begin =
          line.find("define timeperiod {") == std::string::npos;
    } else {
      if (line[0] == '}') {
        wait_time_period_begin = true;
        _applier.add_object(*conf);
        conf = std::make_unique<configuration::Timeperiod>();
        conf_hlp =
            std::make_unique<configuration::timeperiod_helper>(conf.get());
        continue;
      }
      if (line.substr(0, 9) == "\tmonday 3") {
        std::cout << "monday 3..." << std::endl;
      }
      std::string_view line_view = absl::StripAsciiWhitespace(line);
      if (line_view[0] == '#')
        continue;
      std::vector<std::string_view> v =
          absl::StrSplit(line_view, absl::MaxSplits(absl::ByAnyChar(" \t"), 1),
                         absl::SkipWhitespace());
      if (v.size() != 2)
        abort();

      std::string_view key = absl::StripAsciiWhitespace(v[0]);
      std::string_view value = absl::StripAsciiWhitespace(v[1]);
      bool retval = false;
      /* particular cases with hook */
      retval = conf_hlp->hook(key, value);
      if (!retval)
        retval = conf_hlp->set(key, value);
      if (!retval) {
        std::cout << "Unable to parse <<" << line << ">>" << std::endl;
        abort();
      }
    }
  }
}

static const std::vector<test_param> cases = {
    {"24x7", "2021-01-01 04:00:00", "2021-01-01 03:00:00",
     "2021-01-01 04:00:00"},
    {"24x7", "2021-01-02 04:00:00", "2021-01-02 03:00:00",
     "2021-01-02 04:00:00"},
    {"24x7", "2021-01-03 04:00:00", "2021-01-03 03:00:00",
     "2021-01-03 04:00:00"},
    {"24x7", "2021-01-04 04:00:00", "2021-01-04 03:00:00",
     "2021-01-04 04:00:00"},
    {"24x7", "2021-01-05 04:00:00", "2021-01-05 03:00:00",
     "2021-01-05 04:00:00"},
    {"24x7", "2021-01-06 04:00:00", "2021-01-06 03:00:00",
     "2021-01-06 04:00:00"},
    {"24x7", "2021-01-07 04:00:00", "2021-01-07 03:00:00",
     "2021-01-07 04:00:00"},
    {"24x7", "2040-12-31 23:55:00", "2041-01-01 00:00:00",
     "2041-01-01 00:00:00"},
    {"24x7", "2041-01-01 00:00:00", "2040-12-31 23:55:00",
     "2041-01-01 00:00:00"},
    /*french dst*/
    {"24x7", "2022-03-27 02:05:00", "2022-03-27 02:00:01",
     "2022-03-27 02:05:00"},
    {"24x7", "2022-10-30 02:05:00", "2022-10-30 02:30:01",
     "2022-10-30 02:30:01 "},

    {"08h30-19h00", "2020-12-31 19:55:00", "2021-01-01 00:00:00",
     "2021-01-01 07:30:00"},
    {"08h30-19h00", "2020-12-31 07:00:00", "2020-12-31 07:00:00",
     "2020-12-31 07:30:00"},
    {"08h30-19h00", "2021-05-07 10:55:00", "2021-05-07 10:00:00",
     "2021-05-07 10:55:00"},
    {"08h30-19h00", "2021-05-07 10:00:00", "2021-05-07 10:55:00",
     "2021-05-07 10:55:00"},

    {"sauf_02h00-03h30", "2020-12-31 19:55:00", "2020-12-31 19:30:00",
     "2020-12-31 19:55:00"},
    {"sauf_02h00-03h30", "2020-12-31 01:30:00", "2020-12-31 01:30:00",
     "2020-12-31 02:30:00"},
    {"sauf_02h00-03h30", "2020-07-31 00:30:00", "2020-07-31 00:30:00",
     "2020-07-31 01:30:00"},
    {"sauf_02h00-03h30", "2022-10-30 00:30:00", "2020-07-31 00:30:00",
     "2022-10-30 02:30:00"},
    {"sauf_02h00-03h30", "2022-03-27 01:15:00", "2020-03-27 01:15:00",
     "2022-03-27 01:30:00"},

    {"08h30-19h00_semaine", "2020-12-31 19:55:00", "2020-12-31 19:30:00",
     "2021-01-01 07:30:00"},
    {"08h30-19h00_semaine", "2020-07-31 19:55:00", "2020-07-31 19:30:00",
     "2020-08-03 06:30:00"},
    {"08h30-19h00_semaine", "2020-12-31 16:55:00", "2020-12-31 16:30:00",
     "2020-12-31 16:55:00"},

    {"sauf_03h00-05h00", "2020-12-31 19:55:00", "2020-12-31 19:30:00",
     "2020-12-31 19:55:00"},
    {"sauf_03h00-05h00", "2020-12-31 02:55:00", "2020-12-31 01:00:00",
     "2020-12-31 04:00:00"},

    {"Astreinte_Paas_3_avec_feries_2021", "2022-05-07 15:00:00",
     "2022-05-07 15:00:00", "2022-05-13 06:30:00"},
    {"Astreinte_Paas_3_avec_feries_2021", "2022-08-13 03:00:00",
     "2022-08-13 03:00:00", "2022-11-11 07:30:00"},

    {"Astreinte_Paas_1_avec_feries_2021", "2021-04-05 05:00:00",
     "2021-04-05 06:00:00", "2021-04-05 06:30:00"},
    {"Astreinte_Paas_1_avec_feries_2021", "2021-04-05 09:00:00",
     "2021-04-05 07:00:00", "2021-04-05 09:00:00"},
    {"Astreinte_Paas_1_avec_feries_2021", "2021-04-05 06:00:00",
     "2021-04-05 09:00:00", "2021-04-05 09:00:00"}, /* test pb 21.10*/
    {"Astreinte_Paas_1_avec_feries_2021", "2021-11-11 11:00:00",
     "2021-11-11 11:00:00", "2021-11-11 23:00:00"},
    {"Astreinte_Paas_1_avec_feries_2021", "2021-04-06 11:00:00",
     "2021-04-06 11:00:00", "2021-04-06 22:00:00"}

};

INSTANTIATE_TEST_SUITE_P(timeperiod_exception,
                         timeperiod_exception,
                         ::testing::ValuesIn(cases));

TEST_P(timeperiod_exception, TestExceptions) {
  const test_param& param = GetParam();
  set_time(gmt_strtotimet(param.now));
  time_t calculated;

  auto tp_search = timeperiod::timeperiods.find(param.name);
  ASSERT_NE(tp_search, timeperiod::timeperiods.end());

  get_next_valid_time(gmt_strtotimet(param.prefered), &calculated,
                      tp_search->second.get());
  ASSERT_EQ(calculated, gmt_strtotimet(param.expected))
      << " name:" << param.name << " now: " << param.now
      << " pref:" << param.prefered << " expected: " << param.expected
      << " calculated UTC:" << ctime_gmt(calculated);
}
