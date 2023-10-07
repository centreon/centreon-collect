/*
** Copyright 2011-2013,2017 Centreon
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

#ifndef CCE_CONFIGURATION_APPLIER_SERVICEGROUP_HH
#define CCE_CONFIGURATION_APPLIER_SERVICEGROUP_HH

#include <absl/container/flat_hash_set.h>
#include "com/centreon/engine/configuration/servicegroup.hh"
#include "common/configuration/servicegroup_helper.hh"
#include "common/configuration/state.pb.h"

namespace com::centreon::engine {

namespace configuration {
// Forward declarations.
class state;

namespace applier {
class servicegroup {
 public:
  servicegroup() = default;
  servicegroup(const servicegroup&) = delete;
  ~servicegroup() noexcept = default;
  servicegroup& operator=(const servicegroup&) = delete;
  void add_object(const configuration::Servicegroup& obj);
  void add_object(configuration::servicegroup const& obj);
  void expand_objects(configuration::State& s);
  void expand_objects(configuration::state& s);
  void modify_object(configuration::Servicegroup* to_modify,
                     const configuration::Servicegroup& new_object);
  void modify_object(configuration::servicegroup const& obj);
  void remove_object(ssize_t idx);
  void remove_object(configuration::servicegroup const& obj);
  void resolve_object(configuration::servicegroup const& obj);
  void resolve_object(const configuration::Servicegroup& obj);

 private:
  typedef std::map<configuration::servicegroup::key_type,
                   configuration::servicegroup>
      resolved_set;

  void _resolve_members(configuration::servicegroup const& obj,
                        configuration::state const& s);
  void _resolve_members(
      configuration::State& s,
      configuration::Servicegroup* sg_conf,
      absl::flat_hash_set<absl::string_view>& resolved,
      const absl::flat_hash_map<absl::string_view,
                                configuration::Servicegroup*>& sg_by_name);

  resolved_set _resolved;
};
}  // namespace applier
}  // namespace configuration

}  // namespace com::centreon::engine

#endif  // !CCE_CONFIGURATION_APPLIER_SERVICEGROUP_HH
