/*
** Copyright 2011-2014, 2022 Centreon
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

#include "com/centreon/connector/ssh/orders/parser.hh"
#include "com/centreon/connector/log.hh"
#include "com/centreon/connector/ssh/policy.hh"
#include "com/centreon/exceptions/basic.hh"

using namespace com::centreon::connector::ssh::orders;

parser::parser(
    const shared_io_context& io_context,
    const std::shared_ptr<com::centreon::connector::policy_interface>& policy)
    : com::centreon::connector::parser(io_context, policy) {}

parser::pointer parser::create(shared_io_context io_context,
                               const std::shared_ptr<policy_interface>& policy,
                               const std::string& test_cmd_file) {
  pointer ret{new parser(io_context, policy)};

  if (!test_cmd_file.empty()) {
    ret->read_file(test_cmd_file);
  } else {
    ret->start_read();
  }
  return ret;
}

/**
 *  @brief Parse a command.
 *
 *  It is the caller's responsibility to ensure that the command given
 *  to parse is terminated with 4 \0.
 *
 *  @param[in] cmd Command to parse.
 */
void parser::execute(const std::string& cmd) {
  // Get command ID.
  // Note: no need to check npos because cmd is
  //       terminated with at least 4 \0.

  // Find command ID.
  size_t end(cmd.find('\0'));
  char* ptr(nullptr);
  unsigned long long cmd_id(strtoull(cmd.c_str(), &ptr, 10));
  if (!cmd_id || *ptr)
    throw basic_error() << "invalid execution request received:"
                           " bad command ID ("
                        << cmd << ")";
  size_t pos = end + 1;
  // Find timeout value.
  end = cmd.find('\0', pos);
  time_t timeout(static_cast<time_t>(strtoull(cmd.c_str() + pos, &ptr, 10)));

  if (*ptr)
    throw basic_error() << "invalid execution request received:"
                           " bad timeout ("
                        << cmd.c_str() + pos << ")";
  time_point ts_timeout = system_clock::now() + std::chrono::seconds(timeout);
  pos = end + 1;
  // Find start time.
  end = cmd.find('\0', pos);
  time_t start_time(static_cast<time_t>(strtoull(cmd.c_str() + pos, &ptr, 10)));
  if (*ptr || !start_time)
    throw basic_error() << "invalid execution request received:"
                           " bad start time ("
                        << cmd.c_str() + pos << ")";
  pos = end + 1;
  // Find command to execute.
  end = cmd.find('\0', pos);
  std::string cmdline(cmd.substr(pos, end - pos));
  if (cmdline.empty())
    throw basic_error() << "invalid execution request received:"
                           " bad command line ("
                        << cmd.c_str() + pos << ")";
  com::centreon::connector::orders::options::pointer opt(
      std::make_shared<com::centreon::connector::orders::options>());
  try {
    opt->parse(cmdline);
    if (opt->get_commands().empty())
      throw basic_error() << "invalid execution request "
                             "received: bad command line ("
                          << cmd.c_str() + pos << ")";

    if (opt->get_timeout() &&
        opt->get_timeout() < static_cast<unsigned int>(timeout))
      ts_timeout =
          system_clock::now() + std::chrono::seconds(opt->get_timeout());
    else if (opt->get_timeout() > static_cast<unsigned int>(timeout))
      throw basic_error() << "invalid execution request "
                             "received: timeout > to monitoring engine timeout";
  } catch (std::exception const& e) {
    log::core()->error("fail to parse cmd line {} {}", cmdline, e.what());
    _owner->on_error(cmd_id, e.what());
    return;
  }

  // Notify listener.
  _owner->on_execute(cmd_id, ts_timeout, opt);
}
