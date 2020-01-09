/*
** Copyright 2011-2013 Centreon
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

#include "com/centreon/connector/perl/orders/parser.hh"
#include <cstdlib>
#include <cstring>
#include <string>
#include "com/centreon/exceptions/basic.hh"
#include "com/centreon/logging/logger.hh"

using namespace com::centreon::connector::perl::orders;

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
 *  Destructor.
 */
parser::~parser() noexcept {}

/**
 *  Got error event on handle.
 *
 *  @param[in] h Handle.
 */
void parser::error(handle& h) {
  (void)h;
  if (_listnr)
    _listnr->on_error();
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
  log_debug(logging::medium) << "reading data for parsing";
  char buffer[4096];
  unsigned long rb(h.read(buffer, sizeof(buffer)));
  log_debug(logging::medium) << "read " << rb << " bytes from handle";

  // stdin's eof is reached.
  if (!rb) {
    log_debug(logging::high) << "got eof on read handle";
    if (_listnr)
      _listnr->on_eof();
  }
  // Data was read.
  else {
    _buffer.append(buffer, rb);

    // Find a command boundary.
    char boundary[4];
    memset(boundary, 0, sizeof(boundary));
    size_t bound(_buffer.find(boundary, 0, sizeof(boundary)));

    // Parse command.
    while (bound != std::string::npos) {
      log_debug(logging::high) << "got command boundary at offset " << bound;
      bound += sizeof(boundary);
      std::string cmd(_buffer.substr(0, bound));
      _buffer.erase(0, bound);
      bool error(false);
      try {
        _parse(cmd);
      } catch (std::exception const& e) {
        log_error(logging::low) << "orders parsing error: " << e.what();
        error = true;
      } catch (...) {
        log_error(logging::low) << "unknown orders parsing error";
        error = true;
      }
      if (error && _listnr)
        _listnr->on_error();
      bound = _buffer.find(boundary, 0, sizeof(boundary));
    }
  }
}

/**
 *  Do we want to read handle ?
 *
 *  @return Always true.
 */
bool parser::want_read(handle& h) {
  (void)h;
  return true;
}

/**
 *  Do we want to write to handle ?
 *
 *  @return Always false (class just parse).
 */
bool parser::want_write(handle& h) {
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
  unsigned int id(strtoul(cmd.c_str(), NULL, 10));
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
      char* ptr(NULL);
      unsigned long long cmd_id(strtoull(cmd.c_str() + pos, &ptr, 10));
      if (!cmd_id || *ptr)
        throw(basic_error() << "invalid execution request received:"
                               " bad command ID ("
                            << cmd.c_str() + pos << ")");
      pos = end + 1;
      // Find timeout value.
      end = cmd.find('\0', pos);
      time_t timeout(
          static_cast<time_t>(strtoull(cmd.c_str() + pos, &ptr, 10)));
      if (*ptr)
        throw(basic_error() << "invalid execution request received:"
                               " bad timeout ("
                            << cmd.c_str() + pos << ")");
      timeout += time(NULL);
      pos = end + 1;
      // Find start time.
      end = cmd.find('\0', pos);
      strtoull(cmd.c_str() + pos, &ptr, 10);
      if (*ptr)
        throw(basic_error() << "invalid execution request received:"
                               " bad start time ("
                            << cmd.c_str() + pos << ")");
      pos = end + 1;
      // Find command to execute.
      end = cmd.find('\0', pos);
      std::string cmdline(cmd.substr(pos, end - pos));

      // Notify listener.
      if (_listnr)
        _listnr->on_execute(cmd_id, timeout, cmdline);
    } break;
    case 4:  // Quit query.
      if (_listnr)
        _listnr->on_quit();
      break;
    default:
      throw basic_error() << "invalid command received (ID " << id << ")";
  };
}
