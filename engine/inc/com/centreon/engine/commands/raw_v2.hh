/**
 * Copyright 2025 Centreon
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

#ifndef CCE_COMMANDS_RAW_V2_HH
#define CCE_COMMANDS_RAW_V2_HH
#include "com/centreon/common/process/process_args.hh"
#include "com/centreon/engine/commands/command.hh"

namespace com::centreon::engine {

namespace commands {
class environment;

/**
 *  @brief raw_v2 is a specific implementation of command.
 * It uses common::process to start commands
 *
 */
class raw_v2 : public command {
  std::shared_ptr<asio::io_context> _io_context;

  std::atomic_bool _running = false;

  std::string _last_processed_cmd;
  common::process_args::pointer _process_args;

  void _on_complete(uint64_t command_id,
                    time_t start,
                    int exit_code,
                    int exit_status,
                    const std::string& std_out,
                    const std::string& std_err);

 public:
  raw_v2(const std::shared_ptr<asio::io_context> io_context,
         const std::string& name,
         const std::string& command_line,
         command_listener* listener = nullptr);
  raw_v2(const raw_v2&) = delete;
  raw_v2& operator=(const raw_v2&) = delete;

  std::shared_ptr<raw_v2> shared_from_this() {
    return std::static_pointer_cast<raw_v2>(command::shared_from_this());
  }

  uint64_t run(const std::string& process_cmd,
               nagios_macros& macros,
               uint32_t timeout,
               const check_result::pointer& to_push_to_checker,
               const void* caller = nullptr) override;
  void run(const std::string& process_cmd,
           nagios_macros& macros,
           uint32_t timeout,
           result& res) override;
};
}  // namespace commands

}  // namespace com::centreon::engine

#endif  // !CCE_COMMANDS_RAW_HH
