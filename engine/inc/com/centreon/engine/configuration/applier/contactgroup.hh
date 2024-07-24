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
#ifdef LEGACY_CONF
#include "common/engine_legacy_conf/contactgroup.hh"
#else
#include "common/engine_conf/contactgroup_helper.hh"
#endif

namespace com::centreon::engine::configuration {

// Forward declarations.
class state;

namespace applier {
class contactgroup {
#ifdef LEGACY_CONF
  typedef std::map<configuration::contactgroup::key_type,
                   configuration::contactgroup>
      resolved_set;

  resolved_set _resolved;

  void _resolve_members(configuration::state& s,
                        configuration::contactgroup const& obj);
#else
  void _resolve_members(configuration::State& s,
                        configuration::Contactgroup & obj,
			absl::flat_hash_set<std::string_view>& resolved);
#endif

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
#ifdef LEGACY_CONF
  void add_object(configuration::contactgroup const& obj);
  void modify_object(configuration::contactgroup const& obj);
  void remove_object(configuration::contactgroup const& obj);
  void expand_objects(configuration::state& s);
  void resolve_object(configuration::contactgroup const& obj, error_cnt& err);
#else
  void add_object(const configuration::Contactgroup& obj);
  void modify_object(configuration::Contactgroup* to_modify,
                     const configuration::Contactgroup& new_object);
  void remove_object(ssize_t idx);
  void expand_objects(configuration::State& s);
  void resolve_object(const configuration::Contactgroup& obj, error_cnt& err);
#endif
};
}  // namespace applier

}  // namespace com::centreon::engine::configuration

#endif  // !CCE_CONFIGURATION_APPLIER_CONTACTGROUP_HH
