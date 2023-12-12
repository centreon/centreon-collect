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

#ifndef CCE_CONFIGURATION_APPLIER_HOSTESCALATION_HH
#define CCE_CONFIGURATION_APPLIER_HOSTESCALATION_HH

#include "common/configuration/state.pb.h"

namespace com::centreon::engine::configuration {

// Forward declarations.
#ifdef LEGACY_CONF
class hostescalation;
class state;
#else
class Hostescalation;
#endif

namespace applier {
class hostescalation {
#ifdef LEGACY_CONF
  void _expand_hosts(std::set<std::string> const& h,
                     std::set<std::string> const& hg,
                     configuration::state const& s,
                     std::set<std::string>& expanded);
  void _inherits_special_vars(configuration::hostescalation& obj,
                              configuration::state& s);
#endif

 public:
  hostescalation() = default;
  ~hostescalation() noexcept = default;
  hostescalation(hostescalation const&) = delete;
  hostescalation& operator=(hostescalation const&) = delete;
#ifdef LEGACY_CONF
  void add_object(const configuration::hostescalation& obj);
  void modify_object(configuration::hostescalation const& obj);
  void remove_object(configuration::hostescalation const& obj);
  void expand_objects(configuration::state& s);
  void resolve_object(configuration::hostescalation const& obj);
#else
  void add_object(const configuration::Hostescalation& obj);
  void modify_object(configuration::Hostescalation* to_modify,
                     const configuration::Hostescalation& new_object);
  void remove_object(ssize_t idx);
  void expand_objects(configuration::State& s);
  void resolve_object(const configuration::Hostescalation& obj);
#endif
};
}  // namespace applier

}  // namespace com::centreon::engine::configuration

#endif  // !CCE_CONFIGURATION_APPLIER_HOSTESCALATION_HH
