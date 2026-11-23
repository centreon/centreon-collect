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

#ifndef CCB_NEB_REMOTE_ACTIONS_HH
#define CCB_NEB_REMOTE_ACTIONS_HH

#include <string>

namespace com::centreon::broker::neb {

/**
 * @brief Poller side of the downstream channel: executes the RemoteCommand
 * events received from the central on the BBDO stream and answers with
 * RemoteResult events.
 *
 * The BBDO stream calls the handler on the thread that owns it, so the
 * handler only enqueues work on the Engine command_manager (main loop).
 */
class remote_actions {
 public:
  static void load(const std::string& broker_config_file);
  static void unload();
  static void reload_done(bool success, const std::string& message);
};

}  // namespace com::centreon::broker::neb

#endif  // !CCB_NEB_REMOTE_ACTIONS_HH
