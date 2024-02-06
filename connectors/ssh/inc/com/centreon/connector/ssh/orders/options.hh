/**
 * Copyright 2011-2023 Centreon
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

#ifndef CCCS_ORDERS_OPTIONS_HH
#define CCCS_ORDERS_OPTIONS_HH

namespace com::centreon::connector::orders {

/**
 *  @class options options.hh "com/centreon/connector/ssh/orders/options.hh"
 *  @brief Parse orders command line arguments.
 *
 *  Parse orders command line arguments.
 */
class options {
 public:
  using pointer = std::shared_ptr<options>;

  enum ip_protocol { ip_v4 = 0, ip_v6 = 1 };

  options(std::string const& cmdline = "");
  options(options const& p) = delete;
  ~options() noexcept = default;
  options& operator=(options const& p) = delete;
  std::string const& get_authentication() const noexcept;
  std::list<std::string> const& get_commands() const noexcept;
  std::string const& get_host() const noexcept;
  std::string const& get_identity_file() const noexcept;
  ip_protocol get_ip_protocol() const noexcept;
  unsigned short get_port() const noexcept;
  unsigned int get_timeout() const noexcept;
  std::string const& get_user() const noexcept;
  static std::string help();
  void parse(std::string const& cmdline);
  int skip_stderr() const noexcept;
  int skip_stdout() const noexcept;

 private:
  void _copy(options const& p);
  static std::string _get_user_name();

  std::string _authentication;
  std::list<std::string> _commands;
  std::string _host;
  std::string _identity_file;
  ip_protocol _ip_protocol;
  unsigned short _port;
  int _skip_stderr;
  int _skip_stdout;
  unsigned int _timeout;
  std::string _user;
};
}  // namespace com::centreon::connector::orders

#endif  // !CCCS_ORDERS_OPTIONS_HH
