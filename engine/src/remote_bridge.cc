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

#include "com/centreon/engine/remote_bridge.hh"

/* engine_impl.hh, included by command_manager.hh, needs host and service
 * declared first. */
#include "com/centreon/engine/host.hh"
#include "com/centreon/engine/service.hh"

#include "com/centreon/engine/command_manager.hh"
#include "com/centreon/engine/commands/processing.hh"
#include "com/centreon/engine/globals.hh"
#include "com/centreon/broker/neb/remote_actions.hh"

using namespace com::centreon::engine;

void remote_bridge::run_external_command(const std::string& command,
                                         command_done&& done) {
  std::packaged_task<int()> task(
      [command, done = std::move(done)]() -> int {
        /* On the main loop: Engine objects are stable. */
        std::string error;
        int code = commands::processing::execute_with_status(command, &error);
        done(code, error);
        return 0;
      });
  command_manager::instance().enqueue(std::move(task));
}

std::string remote_bridge::main_config_file() {
  return pb_config.cfg_main();
}

void remote_bridge::request_reload() {
  std::packaged_task<int()> task([]() -> int {
    sighup = true;
    return 0;
  });
  command_manager::instance().enqueue(std::move(task));
}

void remote_bridge::request_restart() {
  std::packaged_task<int()> task([]() -> int {
    sigshutdown = true;
    return 0;
  });
  command_manager::instance().enqueue(std::move(task));
}

void remote_bridge::install() {
  com::centreon::broker::neb::remote_actions::engine_bridge b;
  b.run_external_command =
      [](const std::string& command,
         std::function<void(int, const std::string&)>&& done) {
        run_external_command(command, std::move(done));
      };
  b.main_config_file = &main_config_file;
  b.request_reload = &request_reload;
  b.request_restart = &request_restart;
  com::centreon::broker::neb::remote_actions::set_engine_bridge(std::move(b));
}
