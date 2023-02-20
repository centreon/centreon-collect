/*
** Copyright 2011-2013,2017,2023 Centreon
**
** This file is part of Centreon Engine.
**
** Centreon Engine is free software: you can redistribute it and/or
** modify it under the terms of the GNU General Public License version 2
** as published by the Free Software Foundation.
**
** Centreon Engine is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
** General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with Centreon Engine. If not, see
** <http://www.gnu.org/licenses/>.
*/

#ifndef CCE_CONFIGURATION_APPLIER_CONTACTGROUP_HH
#define CCE_CONFIGURATION_APPLIER_CONTACTGROUP_HH

#include <absl/container/flat_hash_map.h>
#include "com/centreon/engine/configuration/contactgroup.hh"
#include "configuration/state.pb.h"

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
  pb_resolved_set _pb_resolved;

  void _resolve_members(configuration::state& s,
                        configuration::contactgroup const& obj);
  void _resolve_members(configuration::State& s,
                        configuration::Contactgroup& obj);

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
  void add_object(const configuration::Contactgroup& obj);
  void expand_objects(configuration::State& s);
  void expand_objects(configuration::state& s);
  void modify_object(configuration::Contactgroup* to_modify,
                     const configuration::Contactgroup& new_object);
  void modify_object(configuration::contactgroup const& obj);
  void remove_object(ssize_t idx);
  void remove_object(configuration::contactgroup const& obj);
  void resolve_object(const configuration::Contactgroup& obj);
  void resolve_object(configuration::contactgroup const& obj);
};
}  // namespace applier
}  // namespace configuration

}

#endif  // !CCE_CONFIGURATION_APPLIER_CONTACTGROUP_HH
