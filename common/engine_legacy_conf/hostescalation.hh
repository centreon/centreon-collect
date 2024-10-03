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
#ifndef CCE_CONFIGURATION_HOSTESCALATION_HH
#define CCE_CONFIGURATION_HOSTESCALATION_HH

#include "com/centreon/common/opt.hh"
#include "group.hh"
#include "object.hh"

using com::centreon::common::opt;

namespace com::centreon::engine {

namespace configuration {
class hostescalation : public object {
 public:
  enum action_on {
    none = 0,
    down = (1 << 0),
    unreachable = (1 << 1),
    recovery = (1 << 2)
  };
  typedef hostescalation key_type;

  hostescalation();
  hostescalation(hostescalation const& right);
  ~hostescalation() throw() override;
  hostescalation& operator=(hostescalation const& right);
  bool operator==(hostescalation const& right) const throw();
  bool operator!=(hostescalation const& right) const throw();
  bool operator<(hostescalation const& right) const;
  void check_validity(error_cnt& err) const override;
  key_type const& key() const throw();
  void merge(object const& obj) override;
  bool parse(char const* key, char const* value) override;

  set_string& contactgroups() throw();
  set_string const& contactgroups() const throw();
  bool contactgroups_defined() const throw();
  void escalation_options(unsigned short options) throw();
  unsigned short escalation_options() const throw();
  void escalation_period(std::string const& period);
  std::string const& escalation_period() const throw();
  bool escalation_period_defined() const throw();
  void first_notification(unsigned int n) throw();
  uint32_t first_notification() const throw();
  set_string& hostgroups() throw();
  set_string const& hostgroups() const throw();
  set_string& hosts() throw();
  set_string const& hosts() const throw();
  void last_notification(unsigned int n) throw();
  unsigned int last_notification() const throw();
  void notification_interval(unsigned int interval);
  unsigned int notification_interval() const throw();
  bool notification_interval_defined() const throw();

 private:
  typedef bool (*setter_func)(hostescalation&, char const*);

  bool _set_contactgroups(std::string const& value);
  bool _set_escalation_options(std::string const& value);
  bool _set_escalation_period(std::string const& value);
  bool _set_first_notification(unsigned int value);
  bool _set_hostgroups(std::string const& value);
  bool _set_hosts(std::string const& value);
  bool _set_last_notification(unsigned int value);
  bool _set_notification_interval(unsigned int value);

  group<set_string> _contactgroups;
  opt<unsigned short> _escalation_options;
  opt<std::string> _escalation_period;
  opt<unsigned int> _first_notification;
  group<set_string> _hostgroups;
  group<set_string> _hosts;
  opt<unsigned int> _last_notification;
  opt<unsigned int> _notification_interval;
  static std::unordered_map<std::string, setter_func> const _setters;
};

size_t hostescalation_key(const hostescalation& he);

typedef std::shared_ptr<hostescalation> hostescalation_ptr;
typedef std::set<hostescalation> set_hostescalation;
}  // namespace configuration

}  // namespace com::centreon::engine

#endif  // !CCE_CONFIGURATION_HOSTESCALATION_HH
