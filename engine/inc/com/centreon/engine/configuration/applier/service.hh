/*
** Copyright 2011-2013,2017, 2023 Centreon
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

#ifndef CCE_CONFIGURATION_APPLIER_SERVICE_HH
#define CCE_CONFIGURATION_APPLIER_SERVICE_HH
#include "common/configuration/state.pb.h"

namespace com::centreon::engine {

namespace configuration {
// Forward declarations.
class service;
class state;

namespace applier {
class service {
  void _expand_service_memberships(configuration::service& obj,
                                   configuration::state& s);
  void _expand_service_memberships(configuration::Service& obj,
                                   configuration::State& s);
  void _inherits_special_vars(configuration::Service& obj,
                              const configuration::State& s);
  void _inherits_special_vars(configuration::service& obj,
                              configuration::state const& s);

 public:
  service() = default;
  service(const service&) = delete;
  ~service() noexcept = default;
  service& operator=(const service&) = delete;
#ifdef LEGACY_CONF
  void add_object(configuration::service const& obj);
  void modify_object(configuration::service const& obj);
  void remove_object(configuration::service const& obj);
  void expand_objects(configuration::state& s);
  void resolve_object(configuration::service const& obj);
#else
  void add_object(const configuration::Service& obj);
  void modify_object(configuration::Service* old_obj,
                     const configuration::Service& new_obj);
  void remove_object(ssize_t idx);
  void expand_objects(configuration::State& s);
  void resolve_object(const configuration::Service& obj);
#endif
};
}  // namespace applier
}  // namespace configuration

}  // namespace com::centreon::engine

#endif  // !CCE_CONFIGURATION_APPLIER_SERVICE_HH
