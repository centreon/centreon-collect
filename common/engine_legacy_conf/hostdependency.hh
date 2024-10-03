/**
 * Copyright 2011-2024 Centreon
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
#ifndef CCE_CONFIGURATION_HOSTDEPENDENCY_HH
#define CCE_CONFIGURATION_HOSTDEPENDENCY_HH

#include "com/centreon/common/opt.hh"
#include "group.hh"
#include "object.hh"

using com::centreon::common::opt;

namespace com::centreon::engine {

namespace configuration {

class hostdependency : public object {
 public:
  enum action_on {
    none = 0,
    up = (1 << 0),
    down = (1 << 1),
    unreachable = (1 << 2),
    pending = (1 << 3)
  };
  enum dependency_kind {
    unknown = 0,
    notification_dependency,
    execution_dependency
  };
  typedef hostdependency key_type;

  hostdependency();
  hostdependency(hostdependency const& right);
  ~hostdependency() noexcept override = default;
  hostdependency& operator=(hostdependency const& right);
  bool operator==(hostdependency const& right) const throw();
  bool operator!=(hostdependency const& right) const throw();
  bool operator<(hostdependency const& right) const;
  void check_validity(error_cnt& err) const override;
  key_type const& key() const throw();
  void merge(object const& obj) override;
  bool parse(char const* key, char const* value) override;

  void dependency_period(std::string const& period);
  std::string const& dependency_period() const throw();
  void dependency_type(dependency_kind type) throw();
  dependency_kind dependency_type() const throw();
  set_string& dependent_hostgroups() throw();
  set_string const& dependent_hostgroups() const throw();
  set_string& dependent_hosts() throw();
  set_string const& dependent_hosts() const throw();
  void execution_failure_options(unsigned int options) throw();
  unsigned int execution_failure_options() const throw();
  set_string& hostgroups() throw();
  set_string const& hostgroups() const throw();
  set_string& hosts() throw();
  set_string const& hosts() const throw();
  void inherits_parent(bool inherit) throw();
  bool inherits_parent() const throw();
  void notification_failure_options(unsigned int options) throw();
  unsigned int notification_failure_options() const throw();

 private:
  typedef bool (*setter_func)(hostdependency&, char const*);

  bool _set_dependency_period(std::string const& value);
  bool _set_dependent_hostgroups(std::string const& value);
  bool _set_dependent_hosts(std::string const& value);
  bool _set_execution_failure_options(std::string const& value);
  bool _set_hostgroups(std::string const& value);
  bool _set_hosts(std::string const& value);
  bool _set_inherits_parent(bool value);
  bool _set_notification_failure_options(std::string const& value);

  std::string _dependency_period;
  dependency_kind _dependency_type;
  group<set_string> _dependent_hostgroups;
  group<set_string> _dependent_hosts;
  opt<unsigned int> _execution_failure_options;
  group<set_string> _hostgroups;
  group<set_string> _hosts;
  opt<bool> _inherits_parent;
  opt<unsigned int> _notification_failure_options;
  static std::unordered_map<std::string, setter_func> const _setters;
};

size_t hostdependency_key(const hostdependency& hd);

typedef std::shared_ptr<hostdependency> hostdependency_ptr;
typedef std::set<hostdependency> set_hostdependency;
}  // namespace configuration

}  // namespace com::centreon::engine

#endif  // !CCE_CONFIGURATION_HOSTDEPENDENCY_HH
