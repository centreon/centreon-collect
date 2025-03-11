/**
 * Copyright 2011-2013 Merethis
 * Copyright 2014-2024 Centreon
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * For more information : contact@centreon.com
 */
#ifndef CCE_CONFIGURATION_APPLIER_GLOBALS_HH
#define CCE_CONFIGURATION_APPLIER_GLOBALS_HH

#include "common/engine_conf/state_helper.hh"

namespace com::centreon::engine::configuration {

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
  void apply(configuration::State& globals);
  static globals& instance();
  void clear();
};
}  // namespace applier

}  // namespace com::centreon::engine::configuration

#endif  // !CCE_CONFIGURATION_APPLIER_GLOBALS_HH
