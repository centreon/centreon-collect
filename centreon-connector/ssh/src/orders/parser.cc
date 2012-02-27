/*
** Copyright 2011-2012 Merethis
**
** This file is part of Centreon Connector SSH.
**
** Centreon Connector SSH is free software: you can redistribute it
** and/or modify it under the terms of the GNU Affero General Public
** License as published by the Free Software Foundation, either version
** 3 of the License, or (at your option) any later version.
**
** Centreon Connector SSH is distributed in the hope that it will be
** useful, but WITHOUT ANY WARRANTY; without even the implied warranty
** of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
** Affero General Public License for more details.
**
** You should have received a copy of the GNU Affero General Public
** License along with Centreon Connector SSH. If not, see
** <http://www.gnu.org/licenses/>.
*/

#include <stdlib.h>
#include <string>
#include <string.h>
#include "com/centreon/connector/ssh/orders/parser.hh"
#include "com/centreon/connector/ssh/orders/options.hh"
#include "com/centreon/exceptions/basic.hh"
#include "com/centreon/logging/logger.hh"

using namespace com::centreon::connector::ssh::orders;

/**************************************
*                                     *
*           Public Methods            *
*                                     *
**************************************/

/**
 *  Default constructor.
 */
parser::parser() : _listnr(NULL) {}

/**
 *  Copy constructor.
 *
 *  @param[in] p Object to copy.
 */
parser::parser(parser const& p) : handle_listener(p) {
  _copy(p);
}

/**
 *  Destructor.
 */
parser::~parser() throw () {}

/**
 *  Assignment operator.
 *
 *  @param[in] p Object to copy.
 *
 *  @return This object.
 */
parser& parser::operator=(parser const& p) {
  if (this != &p) {
    handle_listener::operator=(p);
    _copy(p);
  }
  return (*this);
}

/**
 *  Got error event on handle.
 *
 *  @param[in] h Handle.
 */
void parser::error(handle& h) {
  (void)h;
  if (_listnr)
    _listnr->on_error();
  return ;
}

/**
 *  Get unparsed buffer.
 *
 *  @return Unparsed buffer.
 */
std::string const& parser::get_buffer() const throw () {
  return (_buffer);
}

/**
 *  Get associated listener.
 *
 *  @return Listener if object has one, NULL otherwise.
 */
listener* parser::get_listener() const throw () {
  return (_listnr);
}

/**
 *  Change the listener.
 *
 *  @param[in] l Listener.
 */
void parser::listen(listener* l) throw () {
  _listnr = l;
  return ;
}

/**
 *  Read data from handle.
 *
 *  @param[in] h Handle.
 */
void parser::read(handle& h) {
  // Read data.
  logging::debug(logging::medium) << "reading data for parsing";
  char buffer[4096];
  unsigned long rb(h.read(buffer, sizeof(buffer)));
  logging::debug(logging::medium) << "read "
    << rb << " bytes from handle";

  // stdin's eof is reached.
  if (!rb) {
    logging::debug(logging::high) << "got eof on read handle";
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
      logging::debug(logging::high)
        << "got command boundary at offset " << bound;
      bound += sizeof(boundary);
      std::string cmd(_buffer.substr(0, bound));
      _buffer.erase(0, bound);
      bool error(false);
      try {
        _parse(cmd);
      }
      catch (std::exception const& e) {
        logging::error(logging::low) << "orders parsing error: "
          << e.what();
        error = true;
      }
      catch (...) {
        logging::error(logging::low) << "unknown orders parsing error";
        error = true;
      }
      if (error && _listnr)
        _listnr->on_error();
      bound = _buffer.find(boundary, 0, sizeof(boundary));
    }
  }
  return ;
}

/**
 *  Do we want to read handle ?
 *
 *  @return Always true.
 */
bool parser::want_read(handle& h) {
  (void)h;
  return (true);
}

/**
 *  Do we want to write to handle ?
 *
 *  @return Always false (class just parse).
 */
bool parser::want_write(handle& h) {
  (void)h;
  return (false);
}

/**************************************
*                                     *
*           Private Methods           *
*                                     *
**************************************/

/**
 *  Copy internal data members.
 *
 *  @param[in] p Object to copy.
 */
void parser::_copy(parser const& p) {
  _buffer = p._buffer;
  _listnr = p._listnr;
  return ;
}

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
  case 0: // Version query.
    if (_listnr)
      _listnr->on_version();
    break ;
  case 2: // Execute query.
    {
      // Note: no need to check npos because cmd is
      //       terminated with at least 4 \0.

      // Find command ID.
      size_t end(cmd.find('\0', pos));
      char* ptr(NULL);
      unsigned long long cmd_id(strtoull(cmd.c_str() + pos, &ptr, 10));
      if (!cmd_id || *ptr)
        throw (basic_error() << "invalid execution request received:" \
                    " bad command ID (" << cmd.c_str() + pos << ")");
      pos = end + 1;
      // Find timeout value.
      end = cmd.find('\0', pos);
      time_t timeout(static_cast<time_t>(strtoull(
        cmd.c_str() + pos,
        &ptr,
        10)));
      if (*ptr)
        throw (basic_error() << "invalid execution request received:" \
                    " bad timeout (" << cmd.c_str() + pos << ")");
      timeout += time(NULL);
      pos = end + 1;
      // Find start time.
      end = cmd.find('\0', pos);
      (void)strtoull(cmd.c_str() + pos, &ptr, 10);
      if (*ptr)
        throw (basic_error() << "invalid execution request received:" \
                    " bad start time (" << cmd.c_str() + pos << ")");
      pos = end + 1;
      // Find command to execute.
      end = cmd.find('\0', pos);
      std::string cmdline(cmd.substr(pos, end - pos));
      options opt;
      opt.parse(cmdline);
      if (opt.get_timeout() < timeout)
        timeout = opt.get_timeout();
      else if (opt.get_timeout() > timeout)
        throw (basic_error() << "invalid timeout: check "       \
               "timeout > to monitoring engine timeout");

      // Notify listener.
      if (_listnr)
        _listnr->on_execute(
          cmd_id,
          timeout,
          opt.get_host(),
          opt.get_user(),
          opt.get_authentication(),
          opt.get_identity_file(),
          opt.get_port(),
          opt.get_commands(),
          opt.skip_stdout(),
          opt.skip_stderr(),
          (opt.get_ip_protocol() == options::ip_v6));
    }
    break ;
  case 4: // Quit query.
    if (_listnr)
      _listnr->on_quit();
    break ;
  default:
    throw (basic_error() << "invalid command received (ID "
             << id << ")");
  };
  return ;
}
