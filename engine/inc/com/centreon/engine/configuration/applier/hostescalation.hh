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

#ifndef CCE_CONFIGURATION_APPLIER_HOSTESCALATION_HH
#define CCE_CONFIGURATION_APPLIER_HOSTESCALATION_HH

#include "common/configuration/state.pb.h"

CCE_BEGIN()

namespace configuration {
// Forward declarations.
class Hostescalation;
class hostescalation;
class state;

namespace applier {
class hostescalation {
  void _expand_hosts(std::set<std::string> const& h,
                     std::set<std::string> const& hg,
                     configuration::state const& s,
                     std::set<std::string>& expanded);
  void _inherits_special_vars(configuration::hostescalation& obj,
                              configuration::state& s);

 public:
  hostescalation() = default;
  ~hostescalation() noexcept = default;
  hostescalation(hostescalation const&) = delete;
  hostescalation& operator=(hostescalation const&) = delete;
  void add_object(const configuration::Hostescalation& obj);
  void add_object(const configuration::hostescalation& obj);
  void expand_objects(configuration::State& s);
  void expand_objects(configuration::state& s);
  void modify_object(configuration::Hostescalation* to_modify,
                     const configuration::Hostescalation& new_object);
  void modify_object(configuration::hostescalation const& obj);
  void remove_object(ssize_t idx);
  void remove_object(configuration::hostescalation const& obj);
  void resolve_object(const configuration::Hostescalation& obj);
  void resolve_object(configuration::hostescalation const& obj);
};
}  // namespace applier
}  // namespace configuration

CCE_END()

#endif  // !CCE_CONFIGURATION_APPLIER_HOSTESCALATION_HH
