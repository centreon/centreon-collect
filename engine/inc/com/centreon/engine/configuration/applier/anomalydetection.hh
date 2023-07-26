/*
** Copyright 2020 Centreon
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

#ifndef CCE_CONFIGURATION_APPLIER_ANOMALYDETECTION_HH
#define CCE_CONFIGURATION_APPLIER_ANOMALYDETECTION_HH

#include "common/configuration/state.pb.h"

CCE_BEGIN()

namespace configuration {
// Forward declarations.
class anomalydetection;
class state;

namespace applier {
class anomalydetection {
  void _expand_service_memberships(configuration::anomalydetection& obj,
                                   configuration::state& s);
  void _inherits_special_vars(configuration::anomalydetection& obj,
                              configuration::state const& s);

 public:
  anomalydetection() = default;
  anomalydetection(const anomalydetection&) = delete;
  ~anomalydetection() noexcept = default;
  anomalydetection& operator=(const anomalydetection&) = delete;
  void add_object(const configuration::Anomalydetection& obj);
  void add_object(configuration::anomalydetection const& obj);
  void expand_objects(configuration::State& s);
  void expand_objects(configuration::state& s);
  void modify_object(configuration::Anomalydetection* old_obj,
                     const configuration::Anomalydetection& new_obj);
  void modify_object(configuration::anomalydetection const& obj);
  void remove_object(ssize_t idx);
  void remove_object(configuration::anomalydetection const& obj);
  void resolve_object(const configuration::Anomalydetection& obj);
  void resolve_object(configuration::anomalydetection const& obj);
};
}  // namespace applier
}  // namespace configuration

CCE_END()

#endif  // !CCE_CONFIGURATION_APPLIER_ANOMALYDETECTION_HH
