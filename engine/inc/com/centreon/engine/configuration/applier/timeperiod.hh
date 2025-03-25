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
#include "com/centreon/engine/configuration/indexed_state.hh"

// Forward declaration.
namespace com::centreon::engine::configuration {

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
  void add_object(const configuration::Timeperiod& obj);
  void expand_objects(configuration::indexed_state& s);
  void modify_object(configuration::Timeperiod* to_modify,
                     const configuration::Timeperiod& new_object);
  template <typename Key>
  void remove_object(const std::pair<ssize_t, Key>& p);
  void resolve_object(const configuration::Timeperiod& obj, error_cnt& err);
};
}  // namespace applier
}  // namespace com::centreon::engine::configuration

#endif  // !CCE_CONFIGURATION_APPLIER_TIMEPERIOD_HH
