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
#ifndef CCE_CONFIGURATION_SERVICEDEPENDENCY_HH
#define CCE_CONFIGURATION_SERVICEDEPENDENCY_HH

#include "com/centreon/engine/configuration/group.hh"
#include "com/centreon/engine/configuration/object.hh"
#include "com/centreon/engine/opt.hh"

namespace com::centreon::engine {

namespace configuration {
class servicedependency : public object {
 public:
  enum action_on {
    none = 0,
    ok = (1 << 0),
    unknown = (1 << 1),
    warning = (1 << 2),
    critical = (1 << 3),
    pending = (1 << 4)
  };
  enum dependency_kind {
    unknown_type = 0,
    notification_dependency,
    execution_dependency
  };
  typedef servicedependency key_type;

  servicedependency();
  servicedependency(servicedependency const& right);
  ~servicedependency() noexcept override;
  servicedependency& operator=(servicedependency const& right);
  bool operator==(servicedependency const& right) const noexcept;
  bool operator!=(servicedependency const& right) const noexcept;
  bool operator<(servicedependency const& right) const;
  void check_validity(error_info* err) const override;
  key_type const& key() const noexcept;
  void merge(object const& obj) override;
  bool parse(char const* key, char const* value) override;

  void dependency_period(std::string const& period);
  std::string const& dependency_period() const noexcept;
  void dependency_type(dependency_kind type) noexcept;
  dependency_kind dependency_type() const noexcept;
  list_string& dependent_hostgroups() noexcept;
  list_string const& dependent_hostgroups() const noexcept;
  list_string& dependent_hosts() noexcept;
  list_string const& dependent_hosts() const noexcept;
  list_string& dependent_servicegroups() noexcept;
  list_string const& dependent_servicegroups() const noexcept;
  list_string& dependent_service_description() noexcept;
  list_string const& dependent_service_description() const noexcept;
  void execution_failure_options(unsigned int options) noexcept;
  unsigned int execution_failure_options() const noexcept;
  void inherits_parent(bool inherit) noexcept;
  bool inherits_parent() const noexcept;
  list_string& hostgroups() noexcept;
  list_string const& hostgroups() const noexcept;
  list_string& hosts() noexcept;
  list_string const& hosts() const noexcept;
  void notification_failure_options(unsigned int options) noexcept;
  unsigned int notification_failure_options() const noexcept;
  list_string& servicegroups() noexcept;
  list_string const& servicegroups() const noexcept;
  list_string& service_description() noexcept;
  list_string const& service_description() const noexcept;

 private:
  typedef bool (*setter_func)(servicedependency&, char const*);

  bool _set_dependency_period(std::string const& value);
  bool _set_dependent_hostgroups(std::string const& value);
  bool _set_dependent_hosts(std::string const& value);
  bool _set_dependent_servicegroups(std::string const& value);
  bool _set_dependent_service_description(std::string const& value);
  bool _set_execution_failure_options(std::string const& value);
  bool _set_inherits_parent(bool value);
  bool _set_hostgroups(std::string const& value);
  bool _set_hosts(std::string const& value);
  bool _set_notification_failure_options(std::string const& value);
  bool _set_servicegroups(std::string const& value);
  bool _set_service_description(std::string const& value);

  std::string _dependency_period;
  dependency_kind _dependency_type;
  group<list_string> _dependent_hostgroups;
  group<list_string> _dependent_hosts;
  group<list_string> _dependent_servicegroups;
  group<list_string> _dependent_service_description;
  opt<unsigned int> _execution_failure_options;
  group<list_string> _hostgroups;
  group<list_string> _hosts;
  opt<bool> _inherits_parent;
  opt<unsigned int> _notification_failure_options;
  group<list_string> _servicegroups;
  group<list_string> _service_description;
  static std::unordered_map<std::string, setter_func> const _setters;
};

typedef std::shared_ptr<servicedependency> servicedependency_ptr;
typedef std::set<servicedependency> set_servicedependency;
}  // namespace configuration

}  // namespace com::centreon::engine

#endif  // !CCE_CONFIGURATION_SERVICEDEPENDENCY_HH
