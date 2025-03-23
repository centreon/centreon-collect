/*
 * Copyright 2011-2013,2015,2019-2024 Centreon (https://www.centreon.com/)
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
#ifndef CCE_COMMANDS_FORWARD_HH
#define CCE_COMMANDS_FORWARD_HH

#include "com/centreon/engine/commands/connector.hh"

namespace com::centreon::engine::commands {

/**
 *  @class forward commands/forward.hh
 *  @brief Command is a specific implementation of commands::command.
 *
 *  Command is a specific implementation of commands::command who
 *  provide forward, is more efficiente that a raw command.
 */
class forward : public command {
  std::shared_ptr<command> _s_command;
  command* _command;

 public:
  forward(const std::string& command_name,
          const std::string& command_line,
          const std::shared_ptr<command>& cmd);
  ~forward() noexcept = default;
  forward(const forward&) = delete;
  forward& operator=(const forward&) = delete;
  uint64_t run(const std::string& processed_cmd,
               nagios_macros& macros,
               uint32_t timeout,
               const check_result::pointer& to_push_to_checker,
               const void* caller = nullptr) override;
  void run(const std::string& processed_cmd,
           nagios_macros& macros,
           uint32_t timeout,
           result& res) override;

  std::shared_ptr<command> get_sub_command() const { return _s_command; }

  void register_host_serv(const std::string& host,
                          const std::string& service_description) override;

  void unregister_host_serv(const std::string& host,
                            const std::string& service_description) override;
};

}  // namespace com::centreon::engine::commands

#endif  // !CCE_COMMANDS_FORWARD_HH
