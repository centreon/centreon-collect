/**
 * Copyright 2011-2014 Merethis
 * Copyright 2015-2025 Centreon
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

#ifndef CCE_CONFIGURATION_APPLIER_LOGGING_HH
#define CCE_CONFIGURATION_APPLIER_LOGGING_HH

#include "common/engine_conf/state_helper.hh"

namespace com::centreon::engine {

namespace configuration {
namespace applier {
class logging {
 public:
  void apply(configuration::State& config);
  static logging& instance();
  void clear();

 private:
  logging();
  logging(configuration::State& config);
  logging(logging const&);
  ~logging() noexcept;
  logging& operator=(logging const&);
};
}  // namespace applier
}  // namespace configuration

}  // namespace com::centreon::engine

#endif  // !CCE_CONFIGURATION_APPLIER_LOGGING_HH
