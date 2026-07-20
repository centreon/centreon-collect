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

#ifndef CCE_COMMANDS_RAW_HH
#define CCE_COMMANDS_RAW_HH

#include "com/centreon/engine/commands/command.hh"
#include "com/centreon/process_listener.hh"

namespace com::centreon::engine {

namespace commands {
class environment;

/**
 *  @class raw raw.hh
 *  @brief Raw is a specific implementation of command.
 *
 *  Raw is a specific implementation of command.
 */
class raw : public command, public process_listener {
  std::unordered_map<process*, uint64_t> _processes_busy;
  std::deque<process*> _processes_free;

  void data_is_available(process& p) noexcept override;
  void data_is_available_err(process& p) noexcept override;
  void finished(process& p) noexcept override;
  static void _build_argv_macro_environment(nagios_macros const& macros,
                                            environment& env);
  static void _build_contact_address_environment(nagios_macros const& macros,
                                                 environment& env);
  static void _build_custom_contact_macro_environment(nagios_macros& macros,
                                                      environment& env);
  static void _build_custom_host_macro_environment(nagios_macros& macros,
                                                   environment& env);
  static void _build_custom_service_macro_environment(nagios_macros& macros,
                                                      environment& env);
  static void _build_environment_macros(nagios_macros& macros,
                                        environment& env);
  static void _build_macrosx_environment(nagios_macros& macros,
                                         environment& env);
  process* _get_free_process();

 public:
  raw(const std::string& name,
      const std::string& command_line,
      command_listener* listener = nullptr);
  ~raw() noexcept override;
  raw(const raw&) = delete;
  raw& operator=(const raw&) = delete;
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
