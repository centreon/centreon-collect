/**
 * Copyright 2011-2013 Merethis
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

#ifndef CCE_RETENTION_APPLIER_HOST_HH
#define CCE_RETENTION_APPLIER_HOST_HH

#include "com/centreon/engine/host.hh"
#include "com/centreon/engine/retention/host.hh"

// Forward declaration.
namespace com::centreon::engine {

// Forward declaration.
namespace configuration {
class State;
class state;
}  // namespace configuration

namespace retention {
namespace applier {
class host {
  void _update(const configuration::State& config, const retention::host& state,
               engine::host& obj, bool scheduling_info_is_ok);
  void _update(configuration::state const& config, retention::host const& state,
               engine::host& obj, bool scheduling_info_is_ok);

 public:
  void apply(const configuration::State& config, const list_host& lst,
             bool scheduling_info_is_ok);
  void apply(configuration::state const& config, list_host const& lst,
             bool scheduling_info_is_ok);
};
}  // namespace applier
}  // namespace retention

}  // namespace com::centreon::engine

#endif  // !CCE_RETENTION_APPLIER_HOST_HH
