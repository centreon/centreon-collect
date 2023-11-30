/*
** Copyright 2022 Centreon
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
**
** For more information : contact@centreon.com
*/

#ifndef CCC_CHECKS_RESULT_HH
#define CCC_CHECKS_RESULT_HH


Cnamespace com::centreon {

/**
 *  @class result result.hh "com/centreon/connector/ssh/checks/result.hh"
 *  @brief Check result.
 *
 *  Store check result.
 */
class result {
 public:
  result();
  result(unsigned long long cmd_id, int exit_code, const std::string& error);
  result(unsigned long long cmd_id,
         int exit_code,
         const std::string& output,
         const std::string& error);
  result(result const& r);

  ~result() = default;
  result& operator=(result const& r);
  unsigned long long get_command_id() const noexcept;
  std::string const& get_error() const noexcept;
  bool get_executed() const noexcept;
  int get_exit_code() const noexcept;
  std::string const& get_output() const noexcept;
  void set_command_id(unsigned long long cmd_id) noexcept;
  void set_error(std::string const& error);
  void set_executed(bool executed) noexcept;
  void set_exit_code(int code) noexcept;
  void set_output(std::string const& output);

 private:
  void _internal_copy(result const& r);

  unsigned long long _cmd_id;
  std::string _error;
  bool _executed;
  int _exit_code;
  std::string _output;
};

C}()

#endif  // !CCC_CHECKS_RESULT_HH
