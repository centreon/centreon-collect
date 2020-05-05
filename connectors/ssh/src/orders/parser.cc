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
#include <cstdlib>
#include <cstring>
#include <string>
#include "com/centreon/connector/log.hh"
#include "com/centreon/connector/ssh/orders/options.hh"
#include "com/centreon/exceptions/basic.hh"

using namespace com::centreon::connector::ssh::orders;

/**************************************
 *                                     *
 *           Public Methods            *
 *                                     *
 **************************************/

/**
 *  Default constructor.
 */
parser::parser() : _listnr(nullptr) {}

/**
 *  Got error event on handle.
 *
 *  @param[in] h Handle.
 */
void parser::error([[maybe_unused]]handle& h) {
  if (_listnr)
    _listnr->on_error(0, "error on handle");
}

/**
 *  Get unparsed buffer.
 *
 *  @return Unparsed buffer.
 */
std::string const& parser::get_buffer() const noexcept {
  return _buffer;
}

/**
 *  Get associated listener.
 *
 *  @return Listener if object has one, NULL otherwise.
 */
listener* parser::get_listener() const noexcept {
  return _listnr;
}

/**
 *  Change the listener.
 *
 *  @param[in] l Listener.
 */
void parser::listen(listener* l) noexcept {
  _listnr = l;
}

/**
 *  Read data from handle.
 *
 *  @param[in] h Handle.
 */
void parser::read(handle& h) {
  // Read data.
  log::core()->debug("reading data for parsing");
  char buffer[4096];
  unsigned long rb(h.read(buffer, sizeof(buffer)));
  log::core()->debug("read {} bytes from handle", rb);

  // stdin's eof is reached.
  if (!rb) {
    log::core()->debug("got eof on read handle");
    if (_listnr)
      _listnr->on_eof();
  }
  // Data was read.
  else {
    _buffer.append(buffer, rb);

    // Find a command boundary.
    char boundary[4] { 0, 0, 0, 0 };
    size_t bound(_buffer.find(boundary, 0, sizeof(boundary)));

    // Parse command.
    while (bound != std::string::npos) {
      log::core()->debug("got command boundary at offset {}", bound);
      bound += sizeof(boundary);
      std::string cmd(_buffer.substr(0, bound));
      _buffer.erase(0, bound);
      bool error(false);
      std::string error_msg;
      try {
        _parse(cmd);
      } catch (std::exception const& e) {
        error = true;
        error_msg = "orders parsing error: ";
        error_msg.append(e.what());
        log::core()->error("{}", error_msg);
      } catch (...) {
        error = true;
        error_msg = "unknown orders parsing error";
        log::core()->error("{}", error_msg);
      }
      if (error && _listnr)
        _listnr->on_error(0, error_msg.c_str());
      bound = _buffer.find(boundary, 0, sizeof(boundary));
    }
  }
}

/**
 *  Do we want to read handle ?
 *
 *  @return Always true.
 */
bool parser::want_read([[maybe_unused]]handle& h) {
  (void)h;
  return true;
}

/**
 *  Do we want to write to handle ?
 *
 *  @return Always false (class just parse).
 */
bool parser::want_write([[maybe_unused]]handle& h) {
  (void)h;
  return false;
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

  // Process each command as necessary.
  switch (id) {
    case 0:  // Version query.
      if (_listnr)
        _listnr->on_version();
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
      timestamp ts_timeout = timestamp::now();

      if (*ptr)
        throw basic_error() << "invalid execution request received:"
                               " bad timeout ("
                            << cmd.c_str() + pos << ")";
      ts_timeout += timeout;
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
      options opt;
      try {
        opt.parse(cmdline);
        if (opt.get_commands().empty())
          throw basic_error() << "invalid execution request "
                                 "received: bad command line ("
                              << cmd.c_str() + pos << ")";

        if (opt.get_timeout() &&
            opt.get_timeout() < static_cast<unsigned int>(timeout))
          ts_timeout = timestamp::now() + opt.get_timeout();
        else if (opt.get_timeout() > static_cast<unsigned int>(timeout))
          throw basic_error()
                << "invalid execution request "
                   "received: timeout > to monitoring engine timeout";
      } catch (std::exception const& e) {
        if (_listnr)
          _listnr->on_error(cmd_id, e.what());
        return;
      }

      // Notify listener.
      if (_listnr)
        _listnr->on_execute(cmd_id, ts_timeout, opt.get_host(), opt.get_port(),
                            opt.get_user(), opt.get_authentication(),
                            opt.get_identity_file(), opt.get_commands(),
                            opt.skip_stdout(), opt.skip_stderr(),
                            (opt.get_ip_protocol() == options::ip_v6));
    } break;
    case 4:  // Quit query.
      if (_listnr)
        _listnr->on_quit();
      break;
    default:
      throw basic_error() << "invalid command received (ID " << id << ")";
  };
}
