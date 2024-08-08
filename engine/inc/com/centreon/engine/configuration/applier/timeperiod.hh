/**
 * Copyright 2011-2013,2017-2024 Centreon
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
#ifndef CCE_CONFIGURATION_APPLIER_TIMEPERIOD_HH
#define CCE_CONFIGURATION_APPLIER_TIMEPERIOD_HH

#include "com/centreon/engine/configuration/applier/state.hh"
#include "com/centreon/engine/timeperiod.hh"

#ifndef LEGACY_CONF
#include "common/engine_conf/timeperiod_helper.hh"
#endif

// Forward declaration.
namespace com::centreon::engine::configuration {

#ifdef LEGACY_CONF
// Forward declarations.
class state;
class timeperiod;
#endif

namespace applier {
class timeperiod {
  void _add_exclusions(std::set<std::string> const& exclusions,
                       com::centreon::engine::timeperiod* tp);

 public:
  /**
   * @brief Default constructor.
   */
  timeperiod() = default;

  /**
   * @brief Destructor.
   */
  ~timeperiod() noexcept = default;
  timeperiod(const timeperiod&) = delete;
  timeperiod& operator=(const timeperiod&) = delete;
#ifdef LEGACY_CONF
  void add_object(const configuration::timeperiod& obj);
  void expand_objects(configuration::state& s);
  void modify_object(configuration::timeperiod const& obj);
  void remove_object(configuration::timeperiod const& obj);
  void resolve_object(configuration::timeperiod const& obj, error_cnt& err);
#else
  void add_object(const configuration::Timeperiod& obj);
  void expand_objects(configuration::State& s);
  void modify_object(configuration::Timeperiod* to_modify,
                     const configuration::Timeperiod& new_object);
  void remove_object(ssize_t idx);
  void resolve_object(const configuration::Timeperiod& obj,
                      error_cnt& err);
#endif
};
}  // namespace applier
}  // namespace com::centreon::engine::configuration

#endif  // !CCE_CONFIGURATION_APPLIER_TIMEPERIOD_HH
