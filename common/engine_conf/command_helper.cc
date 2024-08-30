/**
 * Copyright 2022-2024 Centreon (https://www.centreon.com/)
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
#include "common/engine_conf/command_helper.hh"

#include "com/centreon/exceptions/msg_fmt.hh"

using com::centreon::exceptions::msg_fmt;

namespace com::centreon::engine::configuration {

/**
 * @brief Constructor from a Command object.
 *
 * @param obj The Command object on which this helper works. The helper is not
 * the owner of this object. It is just used to set the message default values.
 */
command_helper::command_helper(Command* obj)
    : message_helper(object_type::command,
                     obj,
                     {},
                     Command::descriptor()->field_count()) {
  _init();
}

/**
 * @brief Check the validity of the Command object.
 *
 * @param err An error counter.
 */
void command_helper::check_validity(error_cnt& err) const {
  const Command* o = static_cast<const Command*>(obj());

  if (o->command_name().empty()) {
    err.config_errors++;
    throw msg_fmt("Command has no name (property 'command_name')");
  }
  if (o->command_line().empty()) {
    err.config_errors++;
    throw msg_fmt("Command '{}' has no command line (property 'command_line')",
                  o->command_name());
  }
}

/**
 * @brief The initializer of the Command message.
 */
void command_helper::_init() {
  Command* obj = static_cast<Command*>(mut_obj());
  obj->mutable_obj()->set_register_(true);
}
}  // namespace com::centreon::engine::configuration
