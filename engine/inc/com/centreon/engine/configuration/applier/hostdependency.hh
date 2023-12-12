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

#include "common/configuration/state.pb.h"

namespace com::centreon::engine::configuration {

// Forward declarations.
class hostdependency;
class state;

size_t hostdependency_key(const Hostdependency& hd);
size_t hostdependency_key_l(const hostdependency& hd);

namespace applier {

class hostdependency {
  void _expand_hosts(std::set<std::string> const& hosts,
                     std::set<std::string> const& hostgroups,
                     state& s,
                     std::set<std::string>& expanded);

 public:
  hostdependency() = default;
  hostdependency(const configuration::hostdependency&) = delete;
  ~hostdependency() noexcept = default;
  hostdependency& operator=(const configuration::hostdependency&) = delete;
  void add_object(const configuration::hostdependency& obj);
  void add_object(const Hostdependency& obj);
  void expand_objects(State& s);
  void expand_objects(configuration::state& s);
  void modify_object(Hostdependency* to_modify, const Hostdependency& new_obj);
  void modify_object(configuration::hostdependency const& obj);
  void remove_object(ssize_t idx);
  void remove_object(configuration::hostdependency const& obj);
  void resolve_object(const Hostdependency& obj);
  void resolve_object(configuration::hostdependency const& obj);
};
}  // namespace applier
}  // namespace com::centreon::engine::configuration

#endif  // !CCE_CONFIGURATION_APPLIER_HOSTDEPENDENCY_HH
