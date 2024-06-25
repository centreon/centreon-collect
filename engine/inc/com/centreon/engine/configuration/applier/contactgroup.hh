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
#include "com/centreon/engine/configuration/contactgroup.hh"

namespace com::centreon::engine {

namespace configuration {
// Forward declarations.
class state;

namespace applier {
class contactgroup {
  typedef std::map<configuration::contactgroup::key_type,
                   configuration::contactgroup>
      resolved_set;

  resolved_set _resolved;

  void _resolve_members(configuration::state& s,
                        configuration::contactgroup const& obj);

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
  void add_object(configuration::contactgroup const& obj);
  void modify_object(configuration::contactgroup const& obj);
  void remove_object(configuration::contactgroup const& obj);
  void expand_objects(configuration::state& s);
  void resolve_object(configuration::contactgroup const& obj, error_cnt& err);
};
}  // namespace applier
}  // namespace configuration

}  // namespace com::centreon::engine

#endif  // !CCE_CONFIGURATION_APPLIER_CONTACTGROUP_HH
