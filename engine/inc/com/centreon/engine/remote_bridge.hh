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

#ifndef CCE_REMOTE_BRIDGE_HH
#define CCE_REMOTE_BRIDGE_HH

#include <functional>
#include <string>

/**
 * @brief Engine entry points used by cbmod for the downstream remote actions
 * received on the BBDO stream. This header has no dependency on generated
 * files so that cbmod can include it.
 */
namespace com::centreon::engine::remote_bridge {

using command_done = std::function<void(int code, const std::string& error)>;

/**
 * @brief Run an external command "[ts] CMD;args" on the main loop and call
 * done there with its result (0 when applied).
 */
void run_external_command(const std::string& command, command_done&& done);

/**
 * @brief Path of the main Engine configuration file (centengine.cfg).
 */
std::string main_config_file();

/**
 * @brief Ask the main loop for a configuration reload (as SIGHUP).
 */
void request_reload();

/**
 * @brief Ask the main loop for a clean shutdown (exit code 0), so that the
 * service manager or the container runtime restarts Engine.
 */
void request_restart();

/**
 * @brief Install these functions in cbmod (remote actions), once cbm exists.
 */
void install();

}  // namespace com::centreon::engine::remote_bridge

#endif  // !CCE_REMOTE_BRIDGE_HH
