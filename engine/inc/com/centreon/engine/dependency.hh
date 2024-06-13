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
#ifndef CENTREON_ENGINE_DEPENDENCY_HH
#define CENTREON_ENGINE_DEPENDENCY_HH

#include <com/centreon/engine/timeperiod.hh>

namespace com::centreon::engine {

class dependency {
  /* This key is a hash of the configuration attributes of this hostdependency,
   * essentially used when a new configuration is applied to engine. */
  const size_t _internal_key;

 public:
  enum types { notification = 1, execution };

  dependency(size_t key,
             const std::string& dependent_hostname,
             const std::string& hostname,
             types dependency_type,
             bool inherits_parent,
             bool fail_on_pending,
             const std::string& dependency_period);
  virtual ~dependency() noexcept = default;

  types get_dependency_type() const;
  void set_dependency_type(types dependency_type);
  std::string const& get_dependent_hostname() const;
  void set_dependent_hostname(std::string const& dependent_host_name);
  std::string const& get_hostname() const;
  void set_hostname(std::string const& hostname);
  std::string const& get_dependency_period() const;
  void set_dependency_period(std::string const& dependency_period);
  bool get_inherits_parent() const;
  void set_inherits_parent(bool inherits_parent);
  bool get_fail_on_pending() const;
  void set_fail_on_pending(bool fail_on_pending);
  bool get_circular_path_checked() const;
  void set_circular_path_checked(bool circular_path_checked);
  bool get_contains_circular_path() const;
  void set_contains_circular_path(bool contains_circular_path);
  virtual bool get_fail_on(int state) const = 0;

  virtual bool operator==(dependency const& obj) throw();
  bool operator!=(dependency const& obj) throw();
  virtual bool operator<(dependency const& obj) throw();
  size_t internal_key() const;

  com::centreon::engine::timeperiod* dependency_period_ptr;

 protected:
  types _dependency_type;
  std::string _dependent_hostname;
  std::string _hostname;
  std::string _dependency_period;
  bool _inherits_parent;
  bool _fail_on_pending;
  bool _circular_path_checked;
  bool _contains_circular_path;
};

};  // namespace com::centreon::engine

#endif  // CENTREON_ENGINE_DEPENDENCY_HH
