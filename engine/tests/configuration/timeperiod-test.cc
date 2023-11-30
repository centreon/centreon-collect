/**
 * Copyright 2022 Centreon (https://www.centreon.com/)
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

#include <gtest/gtest.h>

#include "com/centreon/engine/timerange.hh"

#include "com/centreon/engine/common.hh"
#include "com/centreon/engine/configuration/applier/timeperiod.hh"
#include "com/centreon/engine/configuration/timeperiod.hh"
#include "com/centreon/engine/globals.hh"
#include "com/centreon/engine/string.hh"

using namespace com::centreon::engine::configuration;
using namespace com::centreon::engine;

using string_vector = std::vector<std::string>;

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

  const configuration::timeperiod& _parser;
  std::shared_ptr<com::centreon::engine::timeperiod> _result;

  static timerange_list extract_timerange(const std::string& line_content,
                                          unsigned offset,
                                          const std::smatch& datas);
  std::string name, alias;

  std::array<timerange_list, 7> _timeranges;
  std::array<daterange_list, DATERANGE_TYPES> _exceptions;

  group<set_string> _exclude;

 public:
  time_period_comparator(const configuration::timeperiod& parser,
                         const string_vector& timeperiod_content);

  static void extract_skip(const std::smatch matchs,
                           unsigned match_index,
                           daterange& date_range);
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
                                          daterange& date_range) {
  std::smatch skip_extract;
  std::string skip_data = matchs[match_index].str();
  if (std::regex_search(skip_data, skip_extract, skip_extractor)) {
    date_range.set_skip_interval(atoi(skip_extract[1].str().c_str()));
  }
}

time_period_comparator::time_period_comparator(
    const configuration::timeperiod& parser,
    const string_vector& timeperiod_content)
    : _parser(parser) {
  com::centreon::engine::configuration::applier::timeperiod applier;

  com::centreon::engine::timeperiod::timeperiods.clear();

  for (const std::string& line : timeperiod_content) {
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
        timerange_list time_intervals =
            extract_timerange(line, 2, line_extract);
        _timeranges[day_index] = time_intervals;
        continue;
      }
    }
    {  // exception "january 1 08:00-12:00"
      std::smatch line_extract;
      if (std::regex_search(line, line_extract, date_extractor)) {
        timerange_list time_intervals =
            extract_timerange(line, 3, line_extract);
        int day_of_month = atoi(line_extract[2].str().c_str());
        unsigned month_index =
            month_to_index.find(line_extract[1].str())->second;
        daterange toadd(daterange::month_date);
        toadd.set_smon(month_index);
        toadd.set_smday(day_of_month);
        toadd.set_emon(month_index);
        toadd.set_emday(day_of_month);
        toadd.set_timerange(time_intervals);

        _exceptions[daterange::month_date].push_front(toadd);
        continue;
      }
    }
    {  // exception july 10 - 15 / 2			00:00-24:00
      std::smatch line_extract;
      if (std::regex_search(line, line_extract, date_range1_extractor)) {
        timerange_list time_intervals =
            extract_timerange(line, 5, line_extract);
        int day_of_month_start = atoi(line_extract[2].str().c_str());
        int day_of_month_end = atoi(line_extract[3].str().c_str());
        unsigned month_index =
            month_to_index.find(line_extract[1].str())->second;
        daterange toadd(daterange::month_date);
        extract_skip(line_extract, 4, toadd);
        toadd.set_smon(month_index);
        toadd.set_smday(day_of_month_start);
        toadd.set_emon(month_index);
        toadd.set_emday(day_of_month_end);
        toadd.set_timerange(time_intervals);

        _exceptions[daterange::month_date].push_front(toadd);
        continue;
      }
    }
    {  // exception april 10 - may 15 /2	00:00-24:00
      std::smatch line_extract;
      if (std::regex_search(line, line_extract, date_range2_extractor)) {
        timerange_list time_intervals =
            extract_timerange(line, 6, line_extract);
        int day_of_month_start = atoi(line_extract[2].str().c_str());
        unsigned month_index_start =
            month_to_index.find(line_extract[1].str())->second;
        int day_of_month_end = atoi(line_extract[4].str().c_str());
        unsigned month_index_end =
            month_to_index.find(line_extract[3].str())->second;
        daterange toadd(daterange::month_date);
        extract_skip(line_extract, 5, toadd);
        toadd.set_smon(month_index_start);
        toadd.set_smday(day_of_month_start);
        toadd.set_emon(month_index_end);
        toadd.set_emday(day_of_month_end);
        toadd.set_timerange(time_intervals);

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
        daterange toadd(daterange::calendar_date);
        extract_skip(line_extract, 4, toadd);
        timerange_list time_intervals =
            extract_timerange(line, 5, line_extract);
        toadd.set_syear(year);
        toadd.set_eyear(year);
        toadd.set_smon(month);
        toadd.set_emon(month);
        toadd.set_smday(day_of_month);
        toadd.set_emday(day_of_month);
        toadd.set_timerange(time_intervals);

        _exceptions[daterange::calendar_date].push_front(toadd);
        continue;
      }
    }
    {  // exception "2007-01-01 - 2008-02-01 /3	00:00-24:00"
      std::smatch line_extract;
      if (std::regex_search(line, line_extract, full_date_range_extractor)) {
        timerange_list time_intervals =
            extract_timerange(line, 8, line_extract);
        unsigned year_start = atoi(line_extract[1].str().c_str());
        unsigned month_start = atoi(line_extract[2].str().c_str()) - 1;
        unsigned day_of_month_start = atoi(line_extract[3].str().c_str());
        unsigned year_end = atoi(line_extract[4].str().c_str());
        unsigned month_end = atoi(line_extract[5].str().c_str()) - 1;
        unsigned day_of_month_end = atoi(line_extract[6].str().c_str());
        daterange toadd(daterange::calendar_date);
        extract_skip(line_extract, 7, toadd);
        toadd.set_syear(year_start);
        toadd.set_eyear(year_end);
        toadd.set_smon(month_start);
        toadd.set_emon(month_end);
        toadd.set_smday(day_of_month_start);
        toadd.set_emday(day_of_month_end);
        toadd.set_timerange(time_intervals);

        _exceptions[daterange::calendar_date].push_front(toadd);
        continue;
      }
    }
    {  // exception day -1
      std::smatch line_extract;
      if (std::regex_search(line, line_extract, n_th_day_of_month_extractor)) {
        timerange_list time_intervals =
            extract_timerange(line, 2, line_extract);
        unsigned day_of_month = atoi(line_extract[1].str().c_str());
        daterange toadd(daterange::month_day);
        toadd.set_smday(day_of_month);
        toadd.set_emday(day_of_month);
        toadd.set_timerange(time_intervals);

        _exceptions[daterange::month_day].push_front(toadd);
        continue;
      }
    }
    {  // exception day -1
      std::smatch line_extract;
      if (std::regex_search(line, line_extract,
                            n_th_day_of_month_range_extractor)) {
        timerange_list time_intervals =
            extract_timerange(line, 4, line_extract);
        unsigned day_of_month_start = atoi(line_extract[1].str().c_str());
        unsigned day_of_month_end = atoi(line_extract[2].str().c_str());
        daterange toadd(daterange::month_day);
        extract_skip(line_extract, 3, toadd);
        toadd.set_smday(day_of_month_start);
        toadd.set_emday(day_of_month_end);
        toadd.set_timerange(time_intervals);

        _exceptions[daterange::month_day].push_front(toadd);
        continue;
      }
    }
    {  // exception monday 3			00:00-24:00
      std::smatch line_extract;
      if (std::regex_search(line, line_extract, n_th_day_of_week_extractor)) {
        timerange_list time_intervals =
            extract_timerange(line, 3, line_extract);
        daterange toadd(daterange::week_day);
        unsigned week_day_index =
            day_to_index.find(line_extract[1].str())->second;
        int day_month_index = atoi(line_extract[2].str().c_str());
        toadd.set_swday(week_day_index);
        toadd.set_ewday(week_day_index);
        toadd.set_timerange(time_intervals);
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
        timerange_list time_intervals =
            extract_timerange(line, 6, line_extract);
        daterange toadd(daterange::week_day);
        extract_skip(line_extract, 5, toadd);
        unsigned week_day_index_start =
            day_to_index.find(line_extract[1].str())->second;
        int day_month_index_start = atoi(line_extract[2].str().c_str());
        unsigned week_day_index_end =
            day_to_index.find(line_extract[3].str())->second;
        int day_month_index_end = atoi(line_extract[4].str().c_str());
        toadd.set_swday(week_day_index_start);
        toadd.set_ewday(week_day_index_end);
        toadd.set_timerange(time_intervals);
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
        timerange_list time_intervals =
            extract_timerange(line, 4, line_extract);
        daterange toadd(daterange::month_week_day);
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
        toadd.set_timerange(time_intervals);

        _exceptions[daterange::month_week_day].push_front(toadd);
        continue;
      }
    }
    {  // exception tuesday 1 april - friday 2 may / 6	00:00-24:00
      std::smatch line_extract;
      if (std::regex_search(line, line_extract,
                            n_th_day_of_week_of_month_range_extractor)) {
        timerange_list time_intervals =
            extract_timerange(line, 8, line_extract);
        daterange toadd(daterange::month_week_day);
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
        toadd.set_timerange(time_intervals);

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
            _exclude.get().insert(field.substr(1));
          } else {
            _exclude.get().insert(field);
          }
        }
        continue;
      }
    }
    std::cerr << "no match " << line << std::endl;
  }

  applier.add_object(parser);
  _result =
      com::centreon::engine::timeperiod::timeperiods[parser.timeperiod_name()];
}

std::ostream& operator<<(std::ostream& s, const set_string& to_dump) {
  for (const std::string& elem : to_dump) {
    s << ' ' << elem;
  }
  return s;
}

static const char* day_label[] = {"sunday",   "monday", "tuesday", "wednesday",
                                  "thursday", "friday", "saturday"};

std::ostream& operator<<(std::ostream& s, const days_array& to_dump) {
  s << '[';
  for (unsigned day_ind = 0; day_ind < 7; ++day_ind) {
    s << '{' << day_label[day_ind] << " , " << to_dump[day_ind] << "},";
  }
  s << ']';
  return s;
}

template <class T, long unsigned int array_size>
bool operator==(const std::vector<T>& left,
                const std::array<T, array_size>& right) {
  if (left.size() != array_size) {
    std::cerr << "left.size()=" << left.size()
              << " right.size()=" << right.size() << std::endl;
    return false;
  }
  for (unsigned ind = 0; ind < array_size; ++ind) {
    if (left[ind] != right[ind]) {
      return false;
    }
  }
  return true;
}

bool time_period_comparator::is_equal() const {
  if (name != _parser.name()) {
    std::cerr << "different name: " << name << " <> " << _parser.name()
              << std::endl;
    return false;
  }
  if (alias != _parser.alias()) {
    std::cerr << "different alias: " << alias << " <> " << _parser.alias()
              << std::endl;
    return false;
  }

  if (!(_parser.timeranges() == _timeranges)) {
    std::cerr << "timeranges difference" << std::endl;
    std::cerr << "_timeranges=" << _timeranges << std::endl;
    std::cerr << "_parser.timeranges= " << _parser.timeranges() << std::endl;
    return false;
  }

  if (!(_parser.exceptions() == _exceptions)) {
    std::cerr << "exception difference" << std::endl;
    std::cerr << "_exceptions= " << _exceptions << std::endl;
    std::cerr << "_parser.exceptions= " << _parser.exceptions() << std::endl;
    return false;
  }

  if (_exclude.get() != _parser.exclude()) {
    std::cerr << "exception exclude" << std::endl;
    std::cerr << "_exclude=" << _exclude.get() << std::endl;
    std::cerr << "_parser.exclude=" << _parser.exclude() << std::endl;
    return false;
  }

  return true;
}

template <class T1, long unsigned int array_size, class T2>
bool operator==(
    const std::vector<std::list<T1>>& left,
    const std::array<std::list<std::shared_ptr<T2>>, array_size>& right) {
  if (left.size() != array_size) {
    std::cerr << "left.size()=" << left.size()
              << " right.size()=" << right.size() << std::endl;
    return false;
  }
  typename std::vector<std::list<T1>>::const_iterator left_iter = left.begin();
  for (unsigned ind = 0; ind < array_size; ++ind, ++left_iter) {
    const std::list<T1> l_left = *left_iter;
    const std::list<std::shared_ptr<T2>>& l_right = right[ind];
    if (l_left.size() != l_right.size()) {
      std::cerr << "l_left.size()=" << l_left.size()
                << " l_right.size()=" << l_right.size() << std::endl;
      return false;
    }

    typename std::list<T1>::const_iterator l_iter = l_left.begin();
    typename std::list<std::shared_ptr<T2>>::const_iterator r_iter =
        l_right.begin();
    for (; l_iter != l_left.end(); ++l_iter, ++r_iter) {
      if (!(**r_iter == *l_iter)) {
        std::cerr << "**r_iter=" << **r_iter << " *l_iter=" << *l_iter
                  << std::endl;
        return false;
      }
    }
  }
  return true;
}

template <class t_class>
bool operator==(const set_string& left,
                const std::unordered_multimap<std::string, t_class>& right) {
  if (left.size() != right.size()) {
    std::cerr << "left.size()=" << left.size()
              << " right.size()=" << right.size() << std::endl;
    return false;
  }

  std::multiset<std::string> left_to_cmp, right_to_cmp;

  set_string::const_iterator l_iter = left.begin();
  typename std::unordered_multimap<std::string, t_class>::const_iterator
      r_iter = right.begin();
  for (; l_iter != left.end(); ++l_iter, ++r_iter) {
    if (*l_iter != r_iter->first) {
      left_to_cmp.insert(*l_iter);
      right_to_cmp.insert(r_iter->first);
    }
  }
  return left_to_cmp == right_to_cmp;
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

  if (!(_result->days == _timeranges)) {
    std::cerr << "timeranges difference" << std::endl;
    // std::cerr << "_timeranges= " << _timeranges << std::endl;
    std::cerr << "_parser.timeranges= " << _parser.timeranges() << std::endl;
    return false;
  }

  if (!(_result->exceptions == _exceptions)) {
    std::cerr << "exception difference" << std::endl;
    // std::cerr << "_exceptions= " << _exceptions << std::endl;
    std::cerr << "_parser.exceptions= " << _parser.exceptions() << std::endl;
    return false;
  }

  if (!(_exclude.get() == _result->get_exclusions())) {
    std::cerr << "exception exclude" << std::endl;
    std::cerr << "_exclude=" << _exclude.get() << std::endl;
    std::cerr << "_parser.exclude=" << _parser.exclude() << std::endl;
    return false;
  }

  return true;
}

timerange_list time_period_comparator::extract_timerange(
    const std::string& line_content,
    unsigned offset,
    const std::smatch& datas) {
  timerange_list ret;
  for (; offset < datas.size(); ++offset) {
    std::smatch range;
    std::string ranges = datas[offset].str();
    if (ranges.empty()) {
      continue;
    }
    if (std::regex_search(ranges, range, range_extractor)) {
      ret.emplace_back(atoi(range[1].str().c_str()) * 3600 +
                           atoi(range[2].str().c_str()) * 60,
                       atoi(range[3].str().c_str()) * 3600 +
                           atoi(range[4].str().c_str()) * 60);
    } else {
      std::cerr << "fail to parse timerange: " << line_content << std::endl;
    }
  }
  return ret;
}

}  // namespace com::centreon::engine::configuration

std::vector<string_vector> parse_timeperiods_cfg(const std::string& file_path) {
  std::vector<string_vector> ret;

  std::ifstream f(file_path);
  std::string line;

  bool wait_time_period_begin = true;

  string_vector current;
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
        ret.push_back(current);
        current.clear();
        continue;
      }
      current.push_back(com::centreon::engine::string::trim(line));
    }
  }

  return ret;
}

std::vector<string_vector> file_content =
    parse_timeperiods_cfg("tests/timeperiods.cfg");

class timeperiod_config_parser_test
    : public ::testing::TestWithParam<string_vector> {
 protected:
 public:
  static void SetUpTestSuite() {
    config = new com::centreon::engine::configuration::state();
  }
  static void TearDownTestSuite(){};

 protected:
  void SetUp() override {}

  void TearDown() override {}
};

INSTANTIATE_TEST_SUITE_P(timeperiod_config_parser_test,
                         timeperiod_config_parser_test,
                         ::testing::ValuesIn(file_content));

TEST_P(timeperiod_config_parser_test, VerifyParserContent) {
  const string_vector period_content = GetParam();

  com::centreon::engine::configuration::timeperiod parser;
  for (const std::string& to_parse : period_content) {
    parser.parse(to_parse);
  }

  time_period_comparator comparator(parser, period_content);

  ASSERT_TRUE(comparator.is_equal());
  ASSERT_TRUE(comparator.is_result_equal());
}