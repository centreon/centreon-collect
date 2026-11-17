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
#ifndef CCE_OBJECTS_HOSTDEPENDENCY_HH
#define CCE_OBJECTS_HOSTDEPENDENCY_HH

#include "com/centreon/engine/dependency.hh"

/* Forward declaration. */
namespace com::centreon::engine {
class host;
class hostdependency;
class timeperiod;
}  // namespace com::centreon::engine

typedef absl::btree_multimap<
    std::string,
    std::shared_ptr<com::centreon::engine::hostdependency>>
    hostdependency_mmap;

namespace com::centreon::engine {
class hostdependency : public dependency {
 public:
  hostdependency(size_t key,
                 const std::string& dependent_hostname,
                 const std::string& hostname,
                 dependency::types dependency_type,
                 bool inherits_parent,
                 bool fail_on_up,
                 bool fail_on_down,
                 bool fail_on_unreachable,
                 bool fail_on_pending,
                 std::string const& dependency_period);

  bool get_fail_on_up() const;
  void set_fail_on_up(bool fail_on_up);
  bool get_fail_on_down() const;
  void set_fail_on_down(bool fail_on_down);
  bool get_fail_on_unreachable() const;
  void set_fail_on_unreachable(bool fail_on_unreachable);

  bool check_for_circular_hostdependency_path(hostdependency* dep,
                                              types dependency_type);
  void resolve(uint32_t& w, uint32_t& e);
  bool get_fail_on(int state) const override;

  bool operator==(hostdependency const& obj) = delete;
  bool operator<(hostdependency const& obj) throw();

  static hostdependency_mmap hostdependencies;
  static hostdependency_mmap::iterator hostdependencies_find(
      const std::pair<std::string_view, size_t>& key);

  host* master_host_ptr;
  host* dependent_host_ptr;

 private:
  bool _fail_on_up;
  bool _fail_on_down;
  bool _fail_on_unreachable;
};

}  // namespace com::centreon::engine

std::ostream& operator<<(std::ostream& os,
                         com::centreon::engine::hostdependency const& obj);

#endif  // !CCE_OBJECTS_HOSTDEPENDENCY_HH
