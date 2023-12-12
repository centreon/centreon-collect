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

#ifndef CCE_CONFIGURATION_APPLIER_GLOBALS_HH
#define CCE_CONFIGURATION_APPLIER_GLOBALS_HH

#include "com/centreon/engine/configuration/state.hh"
#include "common/configuration/state.pb.h"

namespace com::centreon::engine {

namespace configuration {
namespace applier {
/**
 *  @class globals globals.hh
 *  @brief Simple configuration applier for globals class.
 *
 *  Simple configuration applier for globals class.
 */
class globals {
  /**
   *  Default constructor.
   */
  globals() = default;
  globals(globals const&) = delete;
  ~globals() noexcept;
  globals& operator=(globals const&) = delete;
  void _set_global(char*& property, std::string const& value);

 public:
#ifdef LEGACY_CONF
  void apply(configuration::state& globals);
#else
  void apply(configuration::State& globals);
#endif
  static globals& instance();
  void clear();
};
}  // namespace applier
}  // namespace configuration

}  // namespace com::centreon::engine

#endif  // !CCE_CONFIGURATION_APPLIER_GLOBALS_HH
