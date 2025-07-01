/**
 * Copyright 2022-2024 Centreon (https://www.centreon.com/)
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
#include <fstream>

#include <regex>

#include <absl/strings/ascii.h>
#include <gtest/gtest.h>

#include "com/centreon/engine/common.hh"
#include "com/centreon/engine/configuration/applier/timeperiod.hh"
#include "com/centreon/engine/globals.hh"
#include "common/engine_conf/timeperiod_helper.hh"

using namespace com::centreon::engine::configuration;
using namespace com::centreon::engine;

namespace com::centreon::engine::configuration {
class time_period_comparator {
  static const std::regex name_extractor, alias_extractor, skip_extractor,
      day_extractor, date_extractor, date_range1_extractor,
      date_range2_extractor, range_extractor, full_date_extractor,
      full_date_range_extractor, n_th_day_of_month_extractor,
      n_th_day_of_month_range_extractor, n_th_day_of_week_extractor,
      n_th_day_of_week_range_extractor, n_th_day_of_week_of_month_extractor,
      n_th_day_of_week_of_month_range_extractor, exclude_extractor;

  static const std::map<std::string, unsigned> day_to_index, month_to_index;

  const configuration::Timeperiod& _conf_tp;
  std::shared_ptr<com::centreon::engine::timeperiod> _result;

  static std::list<configuration::Timerange> extract_timerange(
      const std::string& line_content,
      uint32_t offset,
      const std::smatch& datas);
  std::string name, alias;

  /* days_array */
  std::array<std::list<configuration::Timerange>, 7> _timeranges;

  /* The size is Daterange_TypeRange_none, because none is not taken in count in
   * the list. The real last one is week_day of index 4 */
  std::array<std::list<configuration::Daterange>,
             configuration::Daterange_TypeRange_none>
      _exceptions;

  std::set<std::string> _exclude;

 public:
  time_period_comparator(const configuration::Timeperiod& conf_tp,
                         const std::vector<std::string>& timeperiod_content);

  static void extract_skip(const std::smatch matchs,
                           unsigned match_index,
                           Daterange& date_range);
  bool is_equal() const;

  bool is_result_equal() const;

  const std::string get_name() const { return name; }
};

const std::string one_range("\\d\\d:\\d\\d\\-\\d\\d:\\d\\d");
const std::string plage_regex("(" + one_range + ")(," + one_range + ")*");
const std::string full_date("(\\d\\d\\d\\d)-(\\d\\d)-(\\d\\d)");
const std::string months(
    "(january|february|march|april|may|june|july|august|september|october|"
    "november|december)");
const std::string days(
    "(sunday|monday|tuesday|wednesday|thursday|friday|saturday)");

const std::string skip("(\\s+|\\s*/\\s+\\d+)\\s*");
const std::regex time_period_comparator::name_extractor(
    "^name\\s+(\\S+[\\s\\S]+\\S+)");
const std::regex time_period_comparator::alias_extractor(
    "^alias\\s+(\\S+[\\s\\S]+\\S+)");

const std::regex time_period_comparator::skip_extractor("/\\s*(\\d+)");

const std::regex time_period_comparator::day_extractor("^" + days + "\\s+" +
                                                       plage_regex);

const std::regex time_period_comparator::date_extractor("^" + months +
                                                        "\\s+(\\-*\\d+)\\s+" +
                                                        plage_regex);

const std::regex time_period_comparator::date_range1_extractor(
    "^" + months + "\\s+(\\-*\\d+)\\s+\\-\\s+(\\-*\\d+)" + skip + plage_regex);

const std::regex time_period_comparator::date_range2_extractor(
    "^" + months + "\\s+(\\-*\\d+)\\s+\\-\\s+" + months + "\\s+(\\-*\\d+)" +
    skip + plage_regex);

const std::regex time_period_comparator::range_extractor(
    "(\\d\\d):(\\d\\d)\\-(\\d\\d):(\\d\\d)");

const std::regex time_period_comparator::full_date_extractor("^" + full_date +
                                                             skip +
                                                             plage_regex);

const std::regex time_period_comparator::full_date_range_extractor(
    "^" + full_date + "\\s*\\-\\s*" + full_date + skip + plage_regex);

const std::regex time_period_comparator::n_th_day_of_month_extractor(
    "^day\\s+(\\-*\\d+)\\s+" + plage_regex);

const std::regex time_period_comparator::n_th_day_of_month_range_extractor(
    "^day\\s+(\\-*\\d+)\\s+\\-\\s+(\\-*\\d+)" + skip + plage_regex);

const std::regex time_period_comparator::n_th_day_of_week_extractor(
    "^" + days + "\\s+(\\-*\\d+)\\s+" + plage_regex);

const std::regex time_period_comparator::n_th_day_of_week_range_extractor(
    "^" + days + "\\s+(\\-*\\d+)\\s+\\-\\s+" + days + "\\s+(\\-*\\d+)" + skip +
    plage_regex);

const std::regex time_period_comparator::n_th_day_of_week_of_month_extractor(
    "^" + days + "\\s+(\\-*\\d+)\\s+" + months + "\\s+" + plage_regex);

const std::regex
    time_period_comparator::n_th_day_of_week_of_month_range_extractor(
        "^" + days + "\\s+(\\-*\\d+)\\s+" + months + "\\s+\\-\\s+" + days +
        "\\s+(\\-*\\d+)\\s+" + months + skip + plage_regex);

const std::regex time_period_comparator::exclude_extractor(
    "^exclude\\s+([\\w\\-]+)(,[\\w\\-]+)*");

const std::map<std::string, unsigned> time_period_comparator::day_to_index = {
    {"sunday", 0},   {"monday", 1}, {"tuesday", 2}, {"wednesday", 3},
    {"thursday", 4}, {"friday", 5}, {"saturday", 6}};

const std::map<std::string, unsigned> time_period_comparator::month_to_index = {
    {"january", 0},   {"february", 1}, {"march", 2},     {"april", 3},
    {"may", 4},       {"june", 5},     {"july", 6},      {"august", 7},
    {"september", 8}, {"october", 9},  {"november", 10}, {"december", 11}};

void time_period_comparator::extract_skip(const std::smatch matchs,
                                          unsigned match_index,
                                          Daterange& date_range) {
  std::smatch skip_extract;
  std::string skip_data = matchs[match_index].str();
  if (std::regex_search(skip_data, skip_extract, skip_extractor)) {
    date_range.set_skip_interval(atoi(skip_extract[1].str().c_str()));
  }
}

time_period_comparator::time_period_comparator(
    const configuration::Timeperiod& conf_tp,
    const std::vector<std::string>& timeperiod_content)
    : _conf_tp(conf_tp) {
  com::centreon::engine::configuration::applier::timeperiod applier;

  com::centreon::engine::timeperiod::timeperiods.clear();

  for (const std::string& line : timeperiod_content) {
    if (line[0] == '#')
      continue;

    {  // name
      std::smatch line_extract;
      if (std::regex_search(line, line_extract, name_extractor)) {
        name = line_extract[1];
        std::cout << " test " << name << std::endl;
        continue;
      }
    }
    {  // alias
      std::smatch line_extract;
      if (std::regex_search(line, line_extract, alias_extractor)) {
        alias = line_extract[1];
        continue;
      }
    }
    {  // day of week "monday 08:00-12:00"
      std::smatch line_extract;
      if (std::regex_search(line, line_extract, day_extractor)) {
        unsigned day_index = day_to_index.find(line_extract[1].str())->second;
        std::list<configuration::Timerange> time_intervals =
            extract_timerange(line, 2, line_extract);
        _timeranges[day_index] = time_intervals;
        continue;
      }
    }
    {  // exception "january 1 08:00-12:00"
      std::smatch line_extract;
      if (std::regex_search(line, line_extract, date_extractor)) {
        std::list<configuration::Timerange> time_intervals =
            extract_timerange(line, 3, line_extract);
        int day_of_month = atoi(line_extract[2].str().c_str());
        unsigned month_index =
            month_to_index.find(line_extract[1].str())->second;
        Daterange toadd;
        toadd.set_type(Daterange_TypeRange_month_date);
        toadd.set_smon(month_index);
        toadd.set_smday(day_of_month);
        toadd.set_emon(month_index);
        toadd.set_emday(day_of_month);
        for (auto& tr : time_intervals) {
          auto* new_tr = toadd.add_timerange();
          new_tr->CopyFrom(tr);
        }

        _exceptions[daterange::month_date].push_front(toadd);
        continue;
      }
    }
    {  // exception july 10 - 15 / 2			00:00-24:00
      std::smatch line_extract;
      if (std::regex_search(line, line_extract, date_range1_extractor)) {
        std::list<configuration::Timerange> time_intervals =
            extract_timerange(line, 5, line_extract);
        int day_of_month_start = atoi(line_extract[2].str().c_str());
        int day_of_month_end = atoi(line_extract[3].str().c_str());
        unsigned month_index =
            month_to_index.find(line_extract[1].str())->second;
        Daterange toadd;
        toadd.set_type(Daterange_TypeRange_month_date);
        extract_skip(line_extract, 4, toadd);
        toadd.set_smon(month_index);
        toadd.set_smday(day_of_month_start);
        toadd.set_emon(month_index);
        toadd.set_emday(day_of_month_end);
        for (auto& tr : time_intervals) {
          auto* new_tr = toadd.add_timerange();
          new_tr->CopyFrom(tr);
        }

        _exceptions[daterange::month_date].push_front(toadd);
        continue;
      }
    }
    {  // exception april 10 - may 15 /2	00:00-24:00
      std::smatch line_extract;
      if (std::regex_search(line, line_extract, date_range2_extractor)) {
        std::list<configuration::Timerange> time_intervals =
            extract_timerange(line, 6, line_extract);
        int day_of_month_start = atoi(line_extract[2].str().c_str());
        unsigned month_index_start =
            month_to_index.find(line_extract[1].str())->second;
        int day_of_month_end = atoi(line_extract[4].str().c_str());
        unsigned month_index_end =
            month_to_index.find(line_extract[3].str())->second;
        Daterange toadd;
        toadd.set_type(Daterange_TypeRange_month_date);
        extract_skip(line_extract, 5, toadd);
        toadd.set_smon(month_index_start);
        toadd.set_smday(day_of_month_start);
        toadd.set_emon(month_index_end);
        toadd.set_emday(day_of_month_end);
        for (auto& tr : time_intervals) {
          auto* new_tr = toadd.add_timerange();
          new_tr->CopyFrom(tr);
        }

        _exceptions[daterange::month_date].push_front(toadd);
        continue;
      }
    }
    {  // exception "2022-04-05 /5 08:00-12:00"
      std::smatch line_extract;
      if (std::regex_search(line, line_extract, full_date_extractor)) {
        unsigned year = atoi(line_extract[1].str().c_str());
        unsigned month = atoi(line_extract[2].str().c_str()) - 1;
        unsigned day_of_month = atoi(line_extract[3].str().c_str());
        Daterange toadd;
        toadd.set_type(Daterange_TypeRange_calendar_date);
        extract_skip(line_extract, 4, toadd);
        std::list<configuration::Timerange> time_intervals =
            extract_timerange(line, 5, line_extract);
        toadd.set_syear(year);
        toadd.set_eyear(year);
        toadd.set_smon(month);
        toadd.set_emon(month);
        toadd.set_smday(day_of_month);
        toadd.set_emday(day_of_month);
        for (auto& tr : time_intervals) {
          auto* new_tr = toadd.add_timerange();
          new_tr->CopyFrom(tr);
        }

        _exceptions[daterange::calendar_date].push_front(toadd);
        continue;
      }
    }
    {  // exception "2007-01-01 - 2008-02-01 /3	00:00-24:00"
      std::smatch line_extract;
      if (std::regex_search(line, line_extract, full_date_range_extractor)) {
        std::list<configuration::Timerange> time_intervals =
            extract_timerange(line, 8, line_extract);
        unsigned year_start = atoi(line_extract[1].str().c_str());
        unsigned month_start = atoi(line_extract[2].str().c_str()) - 1;
        unsigned day_of_month_start = atoi(line_extract[3].str().c_str());
        unsigned year_end = atoi(line_extract[4].str().c_str());
        unsigned month_end = atoi(line_extract[5].str().c_str()) - 1;
        unsigned day_of_month_end = atoi(line_extract[6].str().c_str());
        Daterange toadd;
        toadd.set_type(Daterange_TypeRange_calendar_date);
        extract_skip(line_extract, 7, toadd);
        toadd.set_syear(year_start);
        toadd.set_eyear(year_end);
        toadd.set_smon(month_start);
        toadd.set_emon(month_end);
        toadd.set_smday(day_of_month_start);
        toadd.set_emday(day_of_month_end);
        for (auto& tr : time_intervals) {
          auto* new_tr = toadd.add_timerange();
          new_tr->CopyFrom(tr);
        }

        _exceptions[daterange::calendar_date].push_front(toadd);
        continue;
      }
    }
    {  // exception day -1
      std::smatch line_extract;
      if (std::regex_search(line, line_extract, n_th_day_of_month_extractor)) {
        std::list<configuration::Timerange> time_intervals =
            extract_timerange(line, 2, line_extract);
        unsigned day_of_month = atoi(line_extract[1].str().c_str());
        Daterange toadd;
        toadd.set_type(Daterange_TypeRange_month_day);
        toadd.set_smday(day_of_month);
        toadd.set_emday(day_of_month);
        for (auto& tr : time_intervals) {
          auto* new_tr = toadd.add_timerange();
          new_tr->CopyFrom(tr);
        }

        _exceptions[daterange::month_day].push_front(toadd);
        continue;
      }
    }
    {  // exception day -1
      std::smatch line_extract;
      if (std::regex_search(line, line_extract,
                            n_th_day_of_month_range_extractor)) {
        std::list<configuration::Timerange> time_intervals =
            extract_timerange(line, 4, line_extract);
        unsigned day_of_month_start = atoi(line_extract[1].str().c_str());
        unsigned day_of_month_end = atoi(line_extract[2].str().c_str());
        Daterange toadd;
        toadd.set_type(Daterange_TypeRange_month_day);
        extract_skip(line_extract, 3, toadd);
        toadd.set_smday(day_of_month_start);
        toadd.set_emday(day_of_month_end);
        for (auto& tr : time_intervals) {
          auto* new_tr = toadd.add_timerange();
          new_tr->CopyFrom(tr);
        }

        _exceptions[daterange::month_day].push_front(toadd);
        continue;
      }
    }
    {  // exception monday 3			00:00-24:00
      std::smatch line_extract;
      if (std::regex_search(line, line_extract, n_th_day_of_week_extractor)) {
        std::list<configuration::Timerange> time_intervals =
            extract_timerange(line, 3, line_extract);
        Daterange toadd;
        toadd.set_type(Daterange_TypeRange_week_day);
        unsigned week_day_index =
            day_to_index.find(line_extract[1].str())->second;
        int day_month_index = atoi(line_extract[2].str().c_str());
        toadd.set_swday(week_day_index);
        toadd.set_ewday(week_day_index);
        for (auto& tr : time_intervals) {
          auto* new_tr = toadd.add_timerange();
          new_tr->CopyFrom(tr);
        }
        toadd.set_swday_offset(day_month_index);
        toadd.set_ewday_offset(day_month_index);

        _exceptions[daterange::week_day].push_front(toadd);
        continue;
      }
    }
    {  // exception monday 3 - thursday 4 / 2		00:00-24:00
      std::smatch line_extract;
      if (std::regex_search(line, line_extract,
                            n_th_day_of_week_range_extractor)) {
        std::list<configuration::Timerange> time_intervals =
            extract_timerange(line, 6, line_extract);
        Daterange toadd;
        toadd.set_type(Daterange_TypeRange_week_day);
        extract_skip(line_extract, 5, toadd);
        unsigned week_day_index_start =
            day_to_index.find(line_extract[1].str())->second;
        int day_month_index_start = atoi(line_extract[2].str().c_str());
        unsigned week_day_index_end =
            day_to_index.find(line_extract[3].str())->second;
        int day_month_index_end = atoi(line_extract[4].str().c_str());
        toadd.set_swday(week_day_index_start);
        toadd.set_ewday(week_day_index_end);
        for (auto& tr : time_intervals) {
          auto* new_tr = toadd.add_timerange();
          new_tr->CopyFrom(tr);
        }
        toadd.set_swday_offset(day_month_index_start);
        toadd.set_ewday_offset(day_month_index_end);

        _exceptions[daterange::week_day].push_front(toadd);
        continue;
      }
    }
    {  // exception thursday -1 november	00:00-24:00
      std::smatch line_extract;
      if (std::regex_search(line, line_extract,
                            n_th_day_of_week_of_month_extractor)) {
        std::list<configuration::Timerange> time_intervals =
            extract_timerange(line, 4, line_extract);
        Daterange toadd;
        toadd.set_type(Daterange_TypeRange_month_week_day);
        unsigned month_index =
            month_to_index.find(line_extract[3].str())->second;
        unsigned week_day_index =
            day_to_index.find(line_extract[1].str())->second;
        int day_month_index = atoi(line_extract[2].str().c_str());
        toadd.set_smon(month_index);
        toadd.set_emon(month_index);
        toadd.set_swday(week_day_index);
        toadd.set_ewday(week_day_index);
        toadd.set_swday_offset(day_month_index);
        toadd.set_ewday_offset(day_month_index);
        for (auto& tr : time_intervals) {
          auto* new_tr = toadd.add_timerange();
          new_tr->CopyFrom(tr);
        }

        _exceptions[daterange::month_week_day].push_front(toadd);
        continue;
      }
    }
    {  // exception tuesday 1 april - friday 2 may / 6	00:00-24:00
      std::smatch line_extract;
      if (std::regex_search(line, line_extract,
                            n_th_day_of_week_of_month_range_extractor)) {
        std::list<configuration::Timerange> time_intervals =
            extract_timerange(line, 8, line_extract);
        Daterange toadd;
        toadd.set_type(Daterange_TypeRange_month_week_day);
        unsigned month_index_start =
            month_to_index.find(line_extract[3].str())->second;
        unsigned week_day_index_start =
            day_to_index.find(line_extract[1].str())->second;
        int day_month_index_start = atoi(line_extract[2].str().c_str());
        unsigned month_index_end =
            month_to_index.find(line_extract[6].str())->second;
        unsigned week_day_index_end =
            day_to_index.find(line_extract[4].str())->second;
        int day_month_index_end = atoi(line_extract[5].str().c_str());
        extract_skip(line_extract, 7, toadd);
        toadd.set_smon(month_index_start);
        toadd.set_emon(month_index_end);
        toadd.set_swday(week_day_index_start);
        toadd.set_ewday(week_day_index_end);
        toadd.set_swday_offset(day_month_index_start);
        toadd.set_ewday_offset(day_month_index_end);
        for (auto& tr : time_intervals) {
          auto* new_tr = toadd.add_timerange();
          new_tr->CopyFrom(tr);
        }

        _exceptions[daterange::month_week_day].push_front(toadd);
        continue;
      }
    }
    {
      std::smatch line_extract;
      if (std::regex_search(line, line_extract, exclude_extractor)) {
        for (std::string field : line_extract) {
          if (field == line_extract[0]) {
            continue;
          }
          if (field.empty()) {
            continue;
          }
          if (field[0] == ',') {
            _exclude.insert(field.substr(1));
          } else {
            _exclude.insert(field);
          }
        }
        continue;
      }
    }
    std::cerr << "no match " << line << std::endl;
  }

  applier.add_object(conf_tp);
  _result =
      com::centreon::engine::timeperiod::timeperiods[conf_tp.timeperiod_name()];
}

std::ostream& operator<<(std::ostream& s,
                         const std::set<std::string>& to_dump) {
  for (const std::string& elem : to_dump) {
    s << ' ' << elem;
  }
  return s;
}

static constexpr std::array<std::string_view, 7> day_label{
    "sunday",   "monday", "tuesday", "wednesday",
    "thursday", "friday", "saturday"};

static std::ostream& operator<<(std::ostream& s,
                                const std::list<configuration::Timerange>& tr) {
  s << '(';
  for (auto& t : tr) {
    s << t.DebugString() << ", ";
  }
  s << ')';
  return s;
}

static std::ostream& operator<<(std::ostream& s,
                                const std::list<configuration::Daterange>& dr) {
  s << '(';
  for (auto& d : dr) {
    s << d.DebugString() << ", ";
  }
  s << ')';
  return s;
}

static std::ostream& operator<<(
    std::ostream& s,
    const std::array<std::list<configuration::Timerange>, 7>& timeranges) {
  s << '[';
  for (unsigned day_ind = 0; day_ind < 7; ++day_ind)
    s << '{' << day_label[day_ind] << ", " << timeranges[day_ind] << "},";
  s << ']';
  return s;
}

static std::ostream& operator<<(
    std::ostream& s,
    const std::array<std::list<configuration::Daterange>,
                     configuration::Daterange_TypeRange_none>& dateranges) {
  s << '[';
  for (unsigned day_ind = 0; day_ind < configuration::Daterange_TypeRange_none;
       ++day_ind)
    s << '{' << day_label[day_ind] << ", " << dateranges[day_ind] << "},";
  s << ']';
  return s;
}

static bool operator==(
    const std::set<std::string>& excl1,
    const std::unordered_multimap<std::string, engine::timeperiod*>& excl2) {
  if (excl1.size() != excl2.size()) {
    std::cerr << "Exclude arrays have not the same size." << std::endl;
    return false;
  }
  return true;
}

static bool operator==(
    const std::array<std::list<configuration::Timerange>, 7>& timerange1,
    const std::array<std::list<engine::timerange>, 7>& timerange2) {
  auto check_timeranges = [](const std::string_view day, auto& day1,
                             auto& day2) -> bool {
    if (day1.size() != day2.size()) {
      std::cerr << day << " timeranges have not the same size: first size: "
                << day1.size() << " ; second size: " << day2.size()
                << std::endl;
      return false;
    }
    for (auto& tr2 : day2) {
      bool found = false;
      for (auto& tr1 : day1) {
        if (tr1.range_start() == tr2.get_range_start() &&
            tr1.range_end() == tr2.get_range_end()) {
          found = true;
          break;
        }
      }
      if (!found) {
        std::cerr << day << " timeranges are not the same." << std::endl;
        return false;
      }
    }
    return true;
  };
  for (int32_t i = 0; i < 7; i++) {
    if (!check_timeranges(day_label[i], timerange1[i], timerange2[i]))
      return false;
  }
  return true;
}

static bool operator==(
    const std::array<std::list<configuration::Timerange>, 7>& timerange1,
    const DaysArray& timerange2) {
  auto check_timeranges = [](const std::string_view day, auto& day1,
                             auto& day2) -> bool {
    if (static_cast<ssize_t>(day1.size()) != day2.size()) {
      std::cerr << "sunday timeranges have not the same size." << std::endl;
      return false;
    }
    for (auto& tr2 : day2) {
      bool found = false;
      for (auto& tr1 : day1) {
        if (tr1.range_start() == tr2.range_start() &&
            tr1.range_end() == tr2.range_end()) {
          found = true;
          break;
        }
      }
      if (!found) {
        std::cerr << day << " timeranges are not the same." << std::endl;
        return false;
      }
    }
    return true;
  };
  if (!check_timeranges("sunday", timerange1[0], timerange2.sunday()) ||
      !check_timeranges("monday", timerange1[1], timerange2.monday()) ||
      !check_timeranges("tuesday", timerange1[2], timerange2.tuesday()) ||
      !check_timeranges("wednesday", timerange1[3], timerange2.wednesday()) ||
      !check_timeranges("thursday", timerange1[4], timerange2.thursday()) ||
      !check_timeranges("friday", timerange1[5], timerange2.friday()) ||
      !check_timeranges("saturday", timerange1[6], timerange2.saturday()))
    return false;
  return true;
}

static bool operator==(const std::set<std::string>& exclude1,
                       const configuration::StringSet& exclude2) {
  if (static_cast<ssize_t>(exclude1.size()) != exclude2.data().size()) {
    std::cerr << "exclude arrays have not the same size " << exclude1.size()
              << " <> " << exclude2.data().size() << std::endl;
    return false;
  }
  for (auto& s : exclude1) {
    bool found = false;
    for (auto& ss : exclude2.data())
      if (ss == s) {
        found = true;
        break;
      }
    if (!found) {
      std::cerr << "exclude sets do not contain the same strings." << std::endl;
      return false;
    }
  }
  return true;
}

static bool operator!=(const std::set<std::string>& exclude1,
                       const configuration::StringSet& exclude2) {
  return !(exclude1 == exclude2);
}

static bool operator==(const Daterange& dr1, const engine::daterange& dr2) {
  if (static_cast<uint32_t>(dr1.type()) !=
      static_cast<uint32_t>(dr2.get_type())) {
    std::cerr << "Dateranges not of the same type." << std::endl;
    return false;
  }
  bool retval =
      dr1.syear() == dr2.get_syear() && dr1.smon() == dr2.get_smon() &&
      dr1.smday() == dr2.get_smday() && dr1.swday() == dr2.get_swday() &&
      dr1.swday_offset() == dr2.get_swday_offset() &&
      dr1.eyear() == dr2.get_eyear() && dr1.emon() == dr2.get_emon() &&
      dr1.emday() == dr2.get_emday() && dr1.ewday() == dr2.get_ewday() &&
      dr1.ewday_offset() == dr2.get_ewday_offset();

  return retval;
}

static bool operator==(
    const std::array<std::list<configuration::Daterange>,
                     configuration::Daterange_TypeRange_none>& exc1,
    const std::array<std::list<engine::daterange>,
                     configuration::Daterange_TypeRange_none>& exc2) {
  auto compare_dateranges =
      [](int32_t idx, const std::list<configuration::Daterange>& lst1,
         const std::list<engine::daterange>& lst2) -> bool {
    for (auto& dr1 : lst1) {
      bool found = false;
      for (auto& dr2 : lst2) {
        if (dr1 == dr2) {
          found = true;
          break;
        }
      }
      if (!found) {
        std::cerr << "Dateranges at index " << idx
                  << " are not equals in exception arrays" << std::endl;
        return false;
      }
    }
    return true;
  };
  for (uint32_t idx = 0; idx < exc1.size(); idx++) {
    if (!compare_dateranges(idx, exc1[idx], exc2[idx]))
      return false;
  }
  return true;
}

static bool operator==(
    const std::array<std::list<configuration::Daterange>,
                     configuration::Daterange_TypeRange_none>& exc1,
    const configuration::ExceptionArray& exc2) {
  auto it_exc1 = exc1.begin();
  auto compare_dateranges =
      [](const std::string_view& name,
         const std::list<configuration::Daterange>& lst,
         const google::protobuf::RepeatedPtrField<configuration::Daterange>&
             rep) -> bool {
    for (auto& dr1 : lst) {
      bool found = false;
      for (auto& dr2 : rep) {
        found = MessageDifferencer::Equals(dr1, dr2);
        if (found)
          break;
      }
      if (!found) {
        std::cerr << "Dateranges '" << name
                  << "' are not equals in exception arrays" << std::endl;
        return false;
      }
    }
    return true;
  };
  if (!compare_dateranges("calendar_date", *it_exc1, exc2.calendar_date()))
    return false;
  ++it_exc1;
  if (!compare_dateranges("month_date", *it_exc1, exc2.month_date()))
    return false;
  ++it_exc1;
  if (!compare_dateranges("month_day", *it_exc1, exc2.month_day()))
    return false;
  ++it_exc1;
  if (!compare_dateranges("month_week_day", *it_exc1, exc2.month_week_day()))
    return false;
  ++it_exc1;
  if (!compare_dateranges("week_day", *it_exc1, exc2.week_day()))
    return false;
  return true;
}

static std::ostream& operator<<(std::ostream& s,
                                const configuration::StringSet& exclude) {
  s << exclude.DebugString();
  return s;
}

bool time_period_comparator::is_equal() const {
  if (name != _conf_tp.timeperiod_name()) {
    std::cerr << "different name: " << name << " <> "
              << _conf_tp.timeperiod_name() << std::endl;
    return false;
  }
  if (alias != _conf_tp.alias()) {
    std::cerr << "different alias: " << alias << " <> " << _conf_tp.alias()
              << std::endl;
    return false;
  }

  if (!(_timeranges == _conf_tp.timeranges())) {
    std::cerr << "timeranges difference" << std::endl;
    std::cerr << "_timeranges=" << _timeranges << std::endl;
    std::cerr << "_conf_tp.timeranges= " << _conf_tp.timeranges().DebugString()
              << std::endl;
    return false;
  }

  if (!(_exceptions == _conf_tp.exceptions())) {
    std::cerr << "exception difference" << std::endl;
    std::cerr << "_exceptions= " << _exceptions << std::endl;
    std::cerr << "_conf_tp.exceptions= " << _conf_tp.exceptions().DebugString()
              << std::endl;
    return false;
  }

  if (_exclude != _conf_tp.exclude()) {
    std::cerr << "exception exclude" << std::endl;
    std::cerr << "_exclude=" << _exclude << std::endl;
    std::cerr << "_conf_tp.exclude=" << _conf_tp.exclude() << std::endl;
    return false;
  }

  return true;
}

bool time_period_comparator::is_result_equal() const {
  if (name != _result->get_name()) {
    std::cerr << "different name: " << name << " <> " << _result->get_name()
              << std::endl;
    return false;
  }
  if (alias != _result->get_alias()) {
    std::cerr << "different alias: " << alias << " <> " << _result->get_alias()
              << std::endl;
    return false;
  }

  if (!(_timeranges == _result->days)) {
    std::cerr << "timeranges difference" << std::endl;
    // std::cerr << "_timeranges= " << _timeranges << std::endl;
    std::cerr << "_conf_tp.timeranges= " << _conf_tp.timeranges().DebugString()
              << std::endl;
    return false;
  }

  if (!(_exceptions == _result->exceptions)) {
    std::cerr << "exception difference" << std::endl;
    // std::cerr << "_exceptions= " << _exceptions << std::endl;
    std::cerr << "_conf_tp.exceptions= " << _conf_tp.exceptions().DebugString()
              << std::endl;
    return false;
  }

  if (!(_exclude == _result->get_exclusions())) {
    std::cerr << "exception exclude" << std::endl;
    std::cerr << "_exclude=" << _exclude << std::endl;
    std::cerr << "_conf_tp.exclude=" << _conf_tp.exclude() << std::endl;
    return false;
  }

  return true;
}

std::list<configuration::Timerange> time_period_comparator::extract_timerange(
    const std::string& line_content,
    uint32_t offset,
    const std::smatch& datas) {
  std::list<configuration::Timerange> ret;
  for (; offset < datas.size(); ++offset) {
    std::smatch range;
    std::string ranges = datas[offset].str();
    if (ranges.empty()) {
      continue;
    }
    if (std::regex_search(ranges, range, range_extractor)) {
      configuration::Timerange t;
      t.set_range_start(atoi(range[1].str().c_str()) * 3600 +
                        atoi(range[2].str().c_str()) * 60);
      t.set_range_end(atoi(range[3].str().c_str()) * 3600 +
                      atoi(range[4].str().c_str()) * 60);
      ret.push_back(std::move(t));
    } else {
      std::cerr << "fail to parse timerange: " << line_content << std::endl;
    }
  }
  return ret;
}

}  // namespace com::centreon::engine::configuration

std::vector<std::vector<std::string>> parse_timeperiods_cfg(
    const std::string& file_path) {
  std::vector<std::vector<std::string>> ret;

  std::ifstream f(file_path);
  std::string line;

  bool wait_time_period_begin = true;

  std::vector<std::string> current;
  while (!f.eof()) {
    std::getline(f, line);

    if (line.empty() || line[0] == '#') {
      continue;
    }

    if (wait_time_period_begin) {
      wait_time_period_begin =
          line.find("define timeperiod {") == std::string::npos;
    } else {
      if (line[0] == '}') {
        wait_time_period_begin = true;
        ret.push_back(current);
        current.clear();
        continue;
      }

      absl::StripAsciiWhitespace(&line);
      current.push_back(std::move(line));
    }
  }

  return ret;
}

std::vector<std::vector<std::string>> file_content =
    parse_timeperiods_cfg("tests/timeperiods.cfg");

class timeperiod_config_parser_test
    : public ::testing::TestWithParam<std::vector<std::string>> {
 protected:
 public:
  static void SetUpTestSuite() { pb_indexed_config.mut_state().Clear(); }
  static void TearDownTestSuite(){};

 protected:
  void SetUp() override {}

  void TearDown() override {}
};

INSTANTIATE_TEST_SUITE_P(timeperiod_config_parser_test,
                         timeperiod_config_parser_test,
                         ::testing::ValuesIn(file_content));

TEST_P(timeperiod_config_parser_test, VerifyParserContent) {
  const std::vector<std::string> period_content = GetParam();

  configuration::Timeperiod conf_tp;
  configuration::timeperiod_helper conf_tp_hlp(&conf_tp);

  for (const std::string& to_parse : period_content) {
    std::string_view line_view = absl::StripAsciiWhitespace(to_parse);
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
    retval = conf_tp_hlp.hook(key, value);
    if (!retval)
      retval = conf_tp_hlp.set(key, value);
    if (!retval) {
      std::cout << "Unable to parse <<" << to_parse << ">>" << std::endl;
      abort();
    }
  }

  time_period_comparator comparator(conf_tp, period_content);

  ASSERT_TRUE(comparator.is_equal());
  ASSERT_TRUE(comparator.is_result_equal());
}
