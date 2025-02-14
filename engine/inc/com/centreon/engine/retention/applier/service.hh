/*
** Copyright 2011-2013 Merethis
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

#ifndef CCE_RETENTION_APPLIER_SERVICE_HH
#define CCE_RETENTION_APPLIER_SERVICE_HH

#include "com/centreon/engine/retention/service.hh"

// Forward declaration.

namespace com::centreon::engine {
class service;

// Forward declaration.
namespace configuration {
class State;
}  // namespace configuration

namespace retention {
namespace applier {
class service {
 public:
  static void apply(const configuration::State& config,
                    const list_service& lst,
                    bool scheduling_info_is_ok);

  static void update(const configuration::State& config,
                     const retention::service& state,
                     com::centreon::engine::service& obj,
                     bool scheduling_info_is_ok);
};
}  // namespace applier
}  // namespace retention

}  // namespace com::centreon::engine

#endif  // !CCE_RETENTION_APPLIER_SERVICE_HH
