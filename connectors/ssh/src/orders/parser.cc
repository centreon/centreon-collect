/*
** Copyright 2011-2014 Centreon
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

parser::parser(shared_io_context io_context,
               const std::shared_ptr<policy_interface>& policy)
    : _io_context(io_context),
      _sin(*io_context, dup(STDIN_FILENO)),
      _dont_care_about_stdin_eof(false),
      _owner(policy) {}

parser::pointer parser::create(shared_io_context io_context,
                               const std::shared_ptr<policy_interface>& policy,
                               const std::string& test_cmd_file) {
  pointer ret(new parser(io_context, policy));

  if (!test_cmd_file.empty()) {
    ret->read_file(test_cmd_file);
  } else {
    ret->start_read();
  }
  return ret;
}

void parser::start_read() {
  log::core()->debug("reading data for parsing");
  _sin.async_read_some(
      asio::buffer(_recv_buff, parser_buff_size),
      [me = shared_from_this()](const std::error_code& error,
                                std::size_t bytes_transferred) {
        me->read_handler(error, bytes_transferred);
      });
}

void parser::read_file(const std::string& test_file_path) {
  std::ifstream file(test_file_path);
  std::stringstream ss;
  ss << file.rdbuf();
  _buffer = ss.str();
  read();
}

const std::error_code parser::eof_err(asio::error::eof,
                                      asio::error::get_misc_category());

void parser::read_handler(const std::error_code& error,
                          std::size_t bytes_transferred) {
  if (_dont_care_about_stdin_eof) {
    return;
  }
  if (error) {
    if (error == eof_err) {  // stdin's eof is reached.

      log::core()->debug("got eof on read handle");
      _owner->on_eof();
      _io_context->stop();
      return;
    }

    log::core()->error("fail to read from stdin {}:{} {}", error.value(),
                       error.category().name(), error.message());
    _owner->on_error(0, "error on handle");
    _io_context->stop();
    return;
  }
  _buffer.append(_recv_buff, bytes_transferred);
  read();
  start_read();
}

/**
 *  Read data
 *
 */
void parser::read() {
  // Find a command boundary.
  constexpr char boundary[4]{0, 0, 0, 0};
  size_t bound(_buffer.find(boundary, 0, sizeof(boundary)));

  // Parse command.
  while (bound != std::string::npos) {
    log::core()->debug("got command boundary at offset {}", bound);
    bound += sizeof(boundary);
    std::string cmd(_buffer.substr(0, bound));
    _buffer.erase(0, bound);
    std::string error_msg;
    try {
      _parse(cmd);
    } catch (std::exception const& e) {
      error_msg = "orders parsing error: ";
      error_msg.append(e.what());
      log::core()->error("{}", error_msg);
      _owner->on_error(0, error_msg);
    } catch (...) {
      error_msg = "unknown orders parsing error";
      log::core()->error("{}", error_msg);
      _owner->on_error(0, error_msg);
    }
    bound = _buffer.find(boundary, 0, sizeof(boundary));
  }
}

/**************************************
 *                                     *
 *           Private Methods           *
 *                                     *
 **************************************/

/**
 *  @brief Parse a command.
 *
 *  It is the caller's responsibility to ensure that the command given
 *  to parse is terminated with 4 \0.
 *
 *  @param[in] cmd Command to parse.
 */
void parser::_parse(std::string const& cmd) {
  // Get command ID.
  size_t pos(cmd.find('\0'));
  unsigned int id(strtoul(cmd.c_str(), nullptr, 10));
  ++pos;

  log::core()->debug("receive cmd {}", id);

  // Process each command as necessary.
  switch (id) {
    case 0:  // Version query.
      _owner->on_version();
      break;
    case 2:  // Execute query.
    {
      // Note: no need to check npos because cmd is
      //       terminated with at least 4 \0.

      // Find command ID.
      size_t end(cmd.find('\0', pos));
      char* ptr(nullptr);
      unsigned long long cmd_id(strtoull(cmd.c_str() + pos, &ptr, 10));
      if (!cmd_id || *ptr)
        throw basic_error() << "invalid execution request received:"
                               " bad command ID ("
                            << cmd.c_str() + pos << ")";
      pos = end + 1;
      // Find timeout value.
      end = cmd.find('\0', pos);
      time_t timeout(
          static_cast<time_t>(strtoull(cmd.c_str() + pos, &ptr, 10)));

      if (*ptr)
        throw basic_error() << "invalid execution request received:"
                               " bad timeout ("
                            << cmd.c_str() + pos << ")";
      time_point ts_timeout =
          system_clock::now() + std::chrono::seconds(timeout);
      pos = end + 1;
      // Find start time.
      end = cmd.find('\0', pos);
      time_t start_time(
          static_cast<time_t>(strtoull(cmd.c_str() + pos, &ptr, 10)));
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
      options::pointer opt(std::make_shared<options>());
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
          throw basic_error()
              << "invalid execution request "
                 "received: timeout > to monitoring engine timeout";
      } catch (std::exception const& e) {
        log::core()->error("fail to parse cmd line {} {}", cmdline, e.what());
        _owner->on_error(cmd_id, e.what());
        return;
      }

      // Notify listener.
      _owner->on_execute(cmd_id, ts_timeout, opt);
    } break;
    case 4:  // Quit query.
      _owner->on_quit();
      break;
    case 10:  // dont care about stdin eof any more
      _dont_care_about_stdin_eof = true;
      break;
    default:
      throw basic_error() << "invalid command received (ID " << id << ")";
  };
}
