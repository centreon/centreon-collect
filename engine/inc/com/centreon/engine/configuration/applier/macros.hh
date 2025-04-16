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
#ifndef CCE_CONFIGURATION_APPLIER_MACROS_HH
#define CCE_CONFIGURATION_APPLIER_MACROS_HH

#include "common/engine_conf/state_helper.hh"

// Forward declaration.
class nagios_macros;

namespace com::centreon::engine {

namespace configuration {
namespace applier {
/**
 *  @class macros macros.hh
 *  @brief Simple configuration applier for macros class.
 *
 *  Simple configuration applier for macros class.
 */
class macros {
 public:
  void apply(configuration::State& config);
  static macros& instance();
  void clear();

 private:
  macros();
  macros(macros const&);
  ~macros() throw();
  macros& operator=(macros const&);
  void _set_macro(unsigned int type, std::string const& value);
  void _set_macros_user(unsigned int idx, std::string const& value);

  nagios_macros* _mac;
};
}  // namespace applier
}  // namespace configuration

}  // namespace com::centreon::engine

#endif  // !CCE_CONFIGURATION_APPLIER_MACROS_HH
