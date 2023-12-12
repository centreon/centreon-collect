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

#ifndef CCE_CONFIGURATION_APPLIER_HOSTDEPENDENCY_HH
#define CCE_CONFIGURATION_APPLIER_HOSTDEPENDENCY_HH

#include "common/configuration/state-generated.pb.h"
#include "common/configuration/state.pb.h"

namespace com::centreon::engine {

namespace configuration {
// Forward declarations.
class hostdependency;
class state;

size_t hostdependency_key(const configuration::Hostdependency& hd);
size_t hostdependency_key_l(const configuration::hostdependency& hd);

namespace applier {

class hostdependency {
  void _expand_hosts(std::set<std::string> const& hosts,
                     std::set<std::string> const& hostgroups,
                     configuration::state& s,
                     std::set<std::string>& expanded);

 public:
  hostdependency() = default;
  hostdependency(const hostdependency&) = delete;
  ~hostdependency() noexcept = default;
  hostdependency& operator=(const hostdependency&) = delete;
#ifdef LEGACY_CONF
  void add_object(configuration::hostdependency const& obj);
  void modify_object(configuration::hostdependency const& obj);
  void remove_object(configuration::hostdependency const& obj);
  void expand_objects(configuration::state& s);
  void resolve_object(configuration::hostdependency const& obj);
#else
  void add_object(const configuration::Hostdependency& obj);
  void modify_object(configuration::Hostdependency* to_modify,
                     const configuration::Hostdependency& new_obj);
  void remove_object(ssize_t idx);
  void expand_objects(configuration::State& s);
  void resolve_object(const configuration::Hostdependency& obj);
#endif
};
}  // namespace applier
}  // namespace configuration

}  // namespace com::centreon::engine

#endif  // !CCE_CONFIGURATION_APPLIER_HOSTDEPENDENCY_HH
