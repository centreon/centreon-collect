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
#ifndef CCE_CONFIGURATION_APPLIER_SERVICEGROUP_HH
#define CCE_CONFIGURATION_APPLIER_SERVICEGROUP_HH

#include "com/centreon/engine/configuration/applier/state.hh"
#include "common/engine_conf/servicegroup_helper.hh"

namespace com::centreon::engine::configuration::applier {

class servicegroup {
 public:
  servicegroup();
  servicegroup(servicegroup const& right);
  ~servicegroup() throw();
  servicegroup& operator=(servicegroup const& right);
  void add_object(const configuration::Servicegroup& obj);
  void modify_object(configuration::Servicegroup* to_modify,
                     const configuration::Servicegroup& new_object);
  template <typename Key>
  void remove_object(const std::pair<ssize_t, Key>& p);
  void resolve_object(const configuration::Servicegroup& obj, error_cnt& err);

 private:
  void _resolve_members(
      configuration::indexed_state& s,
      configuration::Servicegroup* sg_conf,
      absl::flat_hash_set<std::string_view>& resolved,
      const absl::flat_hash_map<std::string_view, configuration::Servicegroup*>&
          sg_by_name);
};
template <>
void servicegroup::remove_object(const std::pair<ssize_t, std::string>& p);
}  // namespace com::centreon::engine::configuration::applier

#endif  // !CCE_CONFIGURATION_APPLIER_SERVICEGROUP_HH
