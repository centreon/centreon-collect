/**
 * Copyright 2011-2013 Merethis
 * Copyright 2014-2024 Centreon
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
#ifndef CCE_OBJECTS_SERVICEDEPENDENCY_HH
#define CCE_OBJECTS_SERVICEDEPENDENCY_HH
#include "com/centreon/engine/dependency.hh"
#include "com/centreon/engine/hash.hh"

/* Forward declaration. */
namespace com::centreon::engine {
class service;
class servicedependency;
class timeperiod;
}  // namespace com::centreon::engine

typedef std::unordered_multimap<
    std::pair<std::string, std::string>,
    std::shared_ptr<com::centreon::engine::servicedependency>,
    pair_hash>
    servicedependency_mmap;

namespace com::centreon::engine {
class servicedependency : public dependency {
  std::string _dependent_service_description;
  std::string _service_description;
  bool _fail_on_ok;
  bool _fail_on_warning;
  bool _fail_on_unknown;
  bool _fail_on_critical;

 public:
  servicedependency(size_t key,
                    std::string const& dependent_host_name,
                    std::string const& dependent_service_description,
                    std::string const& host_name,
                    std::string const& service_description,
                    dependency::types dependency_type,
                    bool inherits_parent,
                    bool fail_on_ok,
                    bool fail_on_warning,
                    bool fail_on_unknown,
                    bool fail_on_critical,
                    bool fail_on_pending,
                    std::string const& dependency_period);

  std::string const& get_dependent_service_description() const;
  void set_dependent_service_description(
      std::string const& dependent_service_desciption);
  std::string const& get_service_description() const;
  void set_service_description(std::string const& service_description);
  bool get_fail_on_ok() const;
  void set_fail_on_ok(bool fail_on_ok);
  bool get_fail_on_warning() const;
  void set_fail_on_warning(bool fail_on_warning);
  bool get_fail_on_unknown() const;
  void set_fail_on_unknown(bool fail_on_unknown);
  bool get_fail_on_critical() const;
  void set_fail_on_critical(bool fail_on_critical);

  bool check_for_circular_servicedependency_path(servicedependency* dep,
                                                 types dependency_type);
  void resolve(uint32_t& w, uint32_t& e);
  bool get_fail_on(int state) const override;

  bool operator==(servicedependency const& obj) = delete;

  service* master_service_ptr;
  service* dependent_service_ptr;

  static servicedependency_mmap servicedependencies;
  static servicedependency_mmap::iterator servicedependencies_find(
      const std::tuple<std::string, std::string, size_t>& key);
};

};  // namespace com::centreon::engine

std::ostream& operator<<(std::ostream& os,
                         com::centreon::engine::servicedependency const& obj);

#endif  // !CCE_OBJECTS_SERVICEDEPENDENCY_HH
