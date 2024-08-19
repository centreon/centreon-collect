/**
 * Copyright 2011-2013,2017-2024 Centreon
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
#ifndef CCE_CONFIGURATION_TIMEPERIOD_HH
#define CCE_CONFIGURATION_TIMEPERIOD_HH

#include "com/centreon/common/opt.hh"
#include "daterange.hh"
#include "group.hh"
#include "object.hh"

namespace com::centreon::engine {

namespace configuration {
namespace test {
class time_period_comparator;
}
class timeperiod : public object {
 public:
  typedef std::string key_type;

  timeperiod(key_type const& key = "");
  timeperiod(timeperiod const& right);
  ~timeperiod() throw() override;
  timeperiod& operator=(timeperiod const& right);
  bool operator==(timeperiod const& right) const;
  bool operator!=(timeperiod const& right) const;
  bool operator<(timeperiod const& right) const;
  void check_validity(error_cnt& err) const override;
  key_type const& key() const throw();
  void merge(object const& obj) override;
  bool parse(char const* key, char const* value) override;
  bool parse(std::string const& line) override;

  std::string const& alias() const throw();
  std::array<std::list<daterange>, daterange::daterange_types> const&
  exceptions() const noexcept;
  set_string const& exclude() const throw();
  std::string const& timeperiod_name() const throw();
  const std::array<std::list<timerange>, 7>& timeranges() const;

  friend test::time_period_comparator;

 private:
  typedef bool (*setter_func)(timeperiod&, char const*);

  bool _add_calendar_date(std::string const& line);
  bool _add_other_date(std::string const& line);
  bool _add_week_day(std::string const& key, std::string const& value);
  static bool _build_timeranges(std::string const& line,
                                std::list<timerange>& timeranges);
  static bool _build_time_t(std::string_view time_str, unsigned long& ret);
  static bool _has_similar_daterange(std::list<daterange> const& lst,
                                     daterange const& range) throw();
  static bool _get_month_id(std::string const& name, unsigned int& id);
  static bool _get_day_id(std::string const& name, unsigned int& id);
  bool _set_alias(std::string const& value);
  bool _set_exclude(std::string const& value);
  bool _set_timeperiod_name(std::string const& value);

  std::string _alias;
  static std::unordered_map<std::string, setter_func> const _setters;
  std::array<std::list<daterange>, daterange::daterange_types> _exceptions;
  group<set_string> _exclude;
  std::string _timeperiod_name;
  std::array<std::list<timerange>, 7> _timeranges;
};

typedef std::shared_ptr<timeperiod> timeperiod_ptr;
typedef std::set<timeperiod> set_timeperiod;
}  // namespace configuration

}  // namespace com::centreon::engine

#endif  // !CCE_CONFIGURATION_TIMEPERIOD_HH
