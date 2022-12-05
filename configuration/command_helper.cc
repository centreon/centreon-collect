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

namespace com::centreon::engine::configuration {
command_helper::command_helper(Command* obj)
    : message_helper(object_type::command, obj, {}, 5) {
  init_command(static_cast<Command*>(mut_obj()));
}

bool command_helper::hook(const absl::string_view& key,
                          const absl::string_view& value) {
  Message* obj = mut_obj();
  return false;
}
}  // namespace com::centreon::engine::configuration
