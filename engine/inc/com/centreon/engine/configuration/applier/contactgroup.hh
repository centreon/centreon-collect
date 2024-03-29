/**
 * Copyright 2011-2013,2017,2023 Centreon
 *
 * This file is part of Centreon Engine.
 *
 * Centreon Engine is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation.
 *
 * Centreon Engine is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Centreon Engine. If not, see
 * <http://www.gnu.org/licenses/>.
 */

#ifndef CCE_CONFIGURATION_APPLIER_CONTACTGROUP_HH
#define CCE_CONFIGURATION_APPLIER_CONTACTGROUP_HH

#include <absl/container/flat_hash_map.h>
#include <absl/container/flat_hash_set.h>

#include "com/centreon/engine/configuration/contactgroup.hh"
#include "common/configuration/state.pb.h"

namespace com::centreon::engine {

namespace configuration {
// Forward declarations.
class state;

using pb_resolved_set =
    absl::flat_hash_map<std::string, configuration::Contactgroup*>;
using resolved_set = std::map<configuration::contactgroup::key_type,
                              configuration::contactgroup>;
namespace applier {
class contactgroup {
  resolved_set _resolved;

  void _resolve_members(configuration::state& s,
                        configuration::contactgroup const& obj);
  void _resolve_members(configuration::State& s,
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
#if LEGACY_CONF
  void add_object(configuration::contactgroup const& obj);
  void modify_object(configuration::contactgroup const& obj);
  void remove_object(configuration::contactgroup const& obj);
  void expand_objects(configuration::state& s);
  void resolve_object(configuration::contactgroup const& obj);
#else
  void add_object(const configuration::Contactgroup& obj);
  void modify_object(configuration::Contactgroup* to_modify,
                     const configuration::Contactgroup& new_object);
  void remove_object(ssize_t idx);
  void expand_objects(configuration::State& s);
  void resolve_object(const configuration::Contactgroup& obj);
#endif
};
}  // namespace applier
}  // namespace configuration

}  // namespace com::centreon::engine

#endif  // !CCE_CONFIGURATION_APPLIER_CONTACTGROUP_HH
