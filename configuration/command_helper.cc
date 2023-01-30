/*
 * Copyright 2022 Centreon (https://www.centreon.com/)
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
#include "configuration/command_helper.hh"
#include "com/centreon/exceptions/msg_fmt.hh"

using msg_fmt = com::centreon::exceptions::msg_fmt;

namespace com {
namespace centreon {
namespace engine {
namespace configuration {

/**
 * @brief Constructor from a Command object.
 *
 * @param obj The Command object on which this helper works. The helper is not
 * the owner of this object.
 */
command_helper::command_helper(Command* obj)
    : message_helper(object_type::command, obj, {}, 5) {
  _init();
}

/**
 * @brief For several keys, the parser of Command objects has a particular
 *        behavior. These behaviors are handled here.
 * @param key The key to parse.
 * @param value The value corresponding to the key
 */
bool command_helper::hook(const absl::string_view& key,
                          const absl::string_view& value) {
  Command* obj = static_cast<Command*>(mut_obj());
  return false;
}

/**
 * @brief Check the validity of the Command object.
 */
void command_helper::check_validity() const {
  const Command* o = static_cast<const Command*>(obj());

  if (o->command_name().empty())
    throw msg_fmt("Command has no name (property 'command_name')");
  if (o->command_line().empty())
    throw msg_fmt("Command '{}' has no command line (property 'command_line')",
                  o->command_name());
}
void command_helper::_init() {}
}  // namespace configuration
}  // namespace engine
}  // namespace centreon

}  // namespace com