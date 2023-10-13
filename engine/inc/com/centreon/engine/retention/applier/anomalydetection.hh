/**
 * Copyright 2022 Centreon
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

#ifndef CCE_RETENTION_APPLIER_ANOMALYDETECTION_HH
#define CCE_RETENTION_APPLIER_ANOMALYDETECTION_HH

#include "com/centreon/engine/retention/anomalydetection.hh"

// Forward declaration.

namespace com::centreon::engine {
class anomalydetection;

// Forward declaration.
namespace configuration {
class state;
class State;
}  // namespace configuration

namespace retention {
namespace applier {
class anomalydetection {
  static void _update(const configuration::State& config,
                      const retention::anomalydetection& state,
                      com::centreon::engine::anomalydetection& obj,
                      bool scheduling_info_is_ok);
  static void _update(const configuration::state& config,
                      const retention::anomalydetection& state,
                      com::centreon::engine::anomalydetection& obj,
                      bool scheduling_info_is_ok);

 public:
  static void apply(configuration::State const& config,
                    list_anomalydetection const& lst,
                    bool scheduling_info_is_ok);
  static void apply(configuration::state const& config,
                    list_anomalydetection const& lst,
                    bool scheduling_info_is_ok);
};
}  // namespace applier
}  // namespace retention

}  // namespace com::centreon::engine

#endif  // !CCE_RETENTION_APPLIER_ANOMALYDETECTION_HH
