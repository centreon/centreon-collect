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
#ifndef CCE_CONFIGURATION_SERVICEESCALATION_HH
#define CCE_CONFIGURATION_SERVICEESCALATION_HH

#include "com/centreon/engine/configuration/group.hh"
#include "com/centreon/engine/configuration/object.hh"
#include "com/centreon/engine/opt.hh"
#include "com/centreon/engine/shared.hh"

namespace com::centreon::engine::configuration {

class serviceescalation : public object {
  typedef bool (*setter_func)(serviceescalation&, char const*);

  group<set_string> _contactgroups;
  opt<unsigned short> _escalation_options;
  opt<std::string> _escalation_period;
  opt<unsigned int> _first_notification;
  group<list_string> _hostgroups;
  group<list_string> _hosts;
  opt<unsigned int> _last_notification;
  opt<unsigned int> _notification_interval;
  group<list_string> _servicegroups;
  group<list_string> _service_description;
  static std::unordered_map<std::string, setter_func> const _setters;
  Uuid _uuid;

  bool _set_contactgroups(std::string const& value);
  bool _set_escalation_options(std::string const& value);
  bool _set_escalation_period(std::string const& value);
  bool _set_first_notification(unsigned int value);
  bool _set_hostgroups(std::string const& value);
  bool _set_hosts(std::string const& value);
  bool _set_last_notification(unsigned int value);
  bool _set_notification_interval(unsigned int value);
  bool _set_servicegroups(std::string const& value);
  bool _set_service_description(std::string const& value);

 public:
  enum action_on {
    none = 0,
    unknown = (1 << 1),
    warning = (1 << 2),
    critical = (1 << 3),
    pending = (1 << 4),
    recovery = (1 << 5)
  };
  typedef serviceescalation key_type;

  serviceescalation();
  serviceescalation(serviceescalation const& right);
  ~serviceescalation() noexcept override = default;
  serviceescalation& operator=(serviceescalation const& right);
  bool operator==(serviceescalation const& right) const noexcept;
  bool operator!=(serviceescalation const& right) const noexcept;
  bool operator<(serviceescalation const& right) const;
  void check_validity() const override;
  key_type const& key() const throw();
  void merge(object const& obj) override;
  bool parse(char const* key, char const* value) override;

  set_string& contactgroups() noexcept;
  set_string const& contactgroups() const noexcept;
  bool contactgroups_defined() const noexcept;
  void escalation_options(unsigned int options) noexcept;
  unsigned short escalation_options() const noexcept;
  void escalation_period(std::string const& period);
  std::string const& escalation_period() const throw();
  bool escalation_period_defined() const throw();
  void first_notification(unsigned int n) throw();
  unsigned int first_notification() const throw();
  list_string& hostgroups() throw();
  list_string const& hostgroups() const throw();
  list_string& hosts() throw();
  list_string const& hosts() const throw();
  void last_notification(unsigned int options) throw();
  unsigned int last_notification() const throw();
  void notification_interval(unsigned int interval) throw();
  unsigned int notification_interval() const throw();
  bool notification_interval_defined() const throw();
  list_string& servicegroups() throw();
  list_string const& servicegroups() const throw();
  list_string& service_description() throw();
  list_string const& service_description() const throw();
  Uuid const& uuid() const;
};

typedef std::shared_ptr<serviceescalation> serviceescalation_ptr;
typedef std::set<serviceescalation> set_serviceescalation;
}  // namespace com::centreon::engine::configuration

#endif  // !CCE_CONFIGURATION_SERVICEESCALATION_HH
