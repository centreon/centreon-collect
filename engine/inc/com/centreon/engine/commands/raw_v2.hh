/*
** Copyright 2011-2013 Merethis
**
** This file is part of Centreon Engine.
**
** Centreon Engine is free software: you can redistribute it and/or
** modify it under the terms of the GNU General Public License version 2
** as published by the Free Software Foundation.
**
** Centreon Engine is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
** General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with Centreon Engine. If not, see
** <http://www.gnu.org/licenses/>.
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
  asio::system_timer _timeout_timer;

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
