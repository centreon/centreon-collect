/**
 * Copyright 2011-2013,2017,2023-2024 Centreon
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * For more information : contact@centreon.com
 *
 */
#ifndef CCE_CONFIGURATION_APPLIER_COMMAND_HH
#define CCE_CONFIGURATION_APPLIER_COMMAND_HH
#include "com/centreon/engine/configuration/applier/state.hh"
#include "common/engine_conf/command_helper.hh"

namespace com::centreon::engine {

// Forward declarations.
namespace commands {
class command;
}

namespace configuration {
// Forward declarations.
class command;
class state;

namespace applier {
class command {
 public:
  command() = default;

  command(command const&) = delete;
  command& operator=(command const&) = delete;

  ~command() noexcept = default;

  void add_object(const configuration::Command& obj);
  void expand_objects(configuration::State& s);
  void modify_object(configuration::Command* to_modify,
                     const configuration::Command& new_obj);
  template <typename Key>
  void remove_object(const std::pair<ssize_t, Key>& p);
  void resolve_object(const configuration::Command& obj, error_cnt& err);
};
template <>
void command::remove_object(const std::pair<ssize_t, std::string>& p);
}  // namespace applier
}  // namespace configuration

}  // namespace com::centreon::engine

#endif  // !CCE_CONFIGURATION_APPLIER_COMMAND_HH
