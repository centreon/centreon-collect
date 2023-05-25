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

#ifndef CCE_CONFIGURATION_APPLIER_SERVICEESCALATION_HH
#define CCE_CONFIGURATION_APPLIER_SERVICEESCALATION_HH

#include "configuration/state-generated.pb.h"

namespace com::centreon::engine {

namespace configuration {
// Forward declarations.
class serviceescalation;
class state;

namespace applier {
class serviceescalation {
  void _expand_services(
      std::list<std::string> const& hst,
      std::list<std::string> const& hg,
      std::list<std::string> const& svc,
      std::list<std::string> const& sg,
      configuration::state& s,
      std::set<std::pair<std::string, std::string> >& expanded);
  void _inherits_special_vars(configuration::serviceescalation& obj,
                              configuration::state const& s);

 public:
  serviceescalation() = default;
  serviceescalation(const serviceescalation&) = delete;
  ~serviceescalation() noexcept = default;
  serviceescalation& operator=(const serviceescalation&) = delete;
  void add_object(const configuration::serviceescalation& obj);
  void add_object(const configuration::Serviceescalation& obj);
  void expand_objects(configuration::state& s);
  void modify_object(const configuration::serviceescalation& obj);
  void remove_object(const configuration::serviceescalation& obj);
  void remove_object(ssize_t idx);
  void resolve_object(const configuration::serviceescalation& obj);
};
}  // namespace applier
}  // namespace configuration

}

#endif  // !CCE_CONFIGURATION_APPLIER_SERVICEESCALATION_HH
