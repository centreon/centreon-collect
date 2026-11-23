/**
 * Copyright 2026 Centreon
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

#ifndef CCC_EXTERNAL_COMMANDS_COMMAND_TABLE_HH
#define CCC_EXTERNAL_COMMANDS_COMMAND_TABLE_HH

#include <optional>
#include <string_view>

namespace com::centreon::common::external_commands {

/**
 * @brief What a legacy external command acts on. This is what Broker needs to
 * route the command to the right poller: a host or service command names its
 * host as first argument, a global command applies to a whole Engine, a
 * process command is addressed to one poller by name.
 *
 * The hostgroup / servicegroup / contact / contactgroup targets only mirror
 * Engine's parser so that the two tables stay in sync (see the Engine unit
 * test): Centreon no longer lets an action be defined on a group, PHP never
 * emits these commands, and Broker refuses them as UNIMPLEMENTED.
 */
enum class target {
  process,       // pilots one Engine process (restart, retention...)
  global,        // platform-wide switch (enable_notifications...)
  host,          // arg 1 = host name
  service,       // arg 1 = host name, arg 2 = service description
  hostgroup,     // arg 1 = hostgroup name (no longer emitted by PHP)
  servicegroup,  // arg 1 = servicegroup name (no longer emitted by PHP)
  contact,       // arg 1 = contact name
  contactgroup,  // arg 1 = contactgroup name
  downtime,      // identified by downtime id (or start time + comment)
  comment,       // identified by comment id
};

struct command_info {
  target kind;
  /* When Broker owns the notification decision (notification_mode=broker), the
   * command is one Broker handles itself: this is the name of the Broker gRPC
   * method that replaces it. Empty when the command is purely a poller matter.
   */
  std::string_view broker_rpc;
};

std::optional<command_info> lookup(std::string_view name);
std::string_view to_string(target t);

}  // namespace com::centreon::common::external_commands

#endif /* !CCC_EXTERNAL_COMMANDS_COMMAND_TABLE_HH */
