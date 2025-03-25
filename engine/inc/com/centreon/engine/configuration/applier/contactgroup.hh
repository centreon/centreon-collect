/**
 * Copyright 2011-2013,2017,2023-2024 Centreon
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
#ifndef CCE_CONFIGURATION_APPLIER_CONTACTGROUP_HH
#define CCE_CONFIGURATION_APPLIER_CONTACTGROUP_HH

#include "com/centreon/engine/configuration/applier/state.hh"
#include "com/centreon/engine/configuration/indexed_state.hh"
#include "common/engine_conf/contactgroup_helper.hh"

namespace com::centreon::engine::configuration {

// Forward declarations.
class state;

namespace applier {
class contactgroup {
  void _resolve_members(configuration::indexed_state& s,
                        configuration::Contactgroup& obj,
                        absl::flat_hash_set<std::string_view>& resolved);

 public:
  /**
   * @brief Default constructor.
   */
  contactgroup() = default;
  /**
   * @brief Destructor.
   */
  ~contactgroup() noexcept = default;
  contactgroup(const contactgroup&) = delete;
  contactgroup& operator=(const contactgroup&) = delete;
  void add_object(const configuration::Contactgroup& obj);
  void modify_object(configuration::Contactgroup* to_modify,
                     const configuration::Contactgroup& new_object);
  template <typename Key>
  void remove_object(const std::pair<ssize_t, Key>& p);
  void expand_objects(configuration::indexed_state& s);
  void resolve_object(const configuration::Contactgroup& obj, error_cnt& err);
};

template <>
void contactgroup::remove_object(const std::pair<ssize_t, std::string>& p);
}  // namespace applier

}  // namespace com::centreon::engine::configuration

#endif  // !CCE_CONFIGURATION_APPLIER_CONTACTGROUP_HH
