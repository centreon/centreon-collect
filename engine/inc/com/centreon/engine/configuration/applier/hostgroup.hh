/**
 * Copyright 2011-2013,2017 Centreon
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

#ifndef CCE_CONFIGURATION_APPLIER_HOSTGROUP_HH
#define CCE_CONFIGURATION_APPLIER_HOSTGROUP_HH

#include "com/centreon/engine/configuration/hostgroup.hh"
#include "common/configuration/state.pb.h"

namespace com::centreon::engine {

namespace configuration {
// Forward declarations.
class state;

namespace applier {
class hostgroup {
 public:
  hostgroup() = default;
  ~hostgroup() noexcept = default;
  hostgroup(const hostgroup&) = delete;
  hostgroup& operator=(const hostgroup&) = delete;
#ifdef LEGACY_CONF
  void add_object(configuration::hostgroup const& obj);
  void modify_object(configuration::hostgroup const& obj);
  void expand_objects(configuration::state& s);
  void remove_object(configuration::hostgroup const& obj);
  void resolve_object(configuration::hostgroup const& obj);
#else
  void add_object(const configuration::Hostgroup& obj);
  void expand_objects(configuration::State& s);
  void modify_object(configuration::Hostgroup* old_obj,
                     const configuration::Hostgroup& new_obj);
  void remove_object(ssize_t idx);
  void resolve_object(const configuration::Hostgroup& obj);
#endif

 private:
  typedef std::map<configuration::hostgroup::key_type, configuration::hostgroup>
      resolved_set;

#ifdef LEGACY_CONF
  void _resolve_members(configuration::state& s,
                        configuration::hostgroup const& obj);

  resolved_set _resolved;
#endif
};
}  // namespace applier
}  // namespace configuration

}  // namespace com::centreon::engine

#endif  // !CCE_CONFIGURATION_APPLIER_HOSTGROUP_HH
