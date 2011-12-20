/*
** Copyright 2011 Merethis
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

#include <assert.h>
#include <signal.h>
#include <sstream>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "com/centreon/connector/ssh/commander.hh"
#include "com/centreon/connector/ssh/multiplexer.hh"
#include "com/centreon/exceptions/basic.hh"
#include "com/centreon/logging/logger.hh"

using namespace com::centreon;
using namespace com::centreon::connector::ssh;

// Class instance.
std::auto_ptr<commander> commander::_instance;

/**************************************
*                                     *
*           Public Methods            *
*                                     *
**************************************/

/**
 *  Destructor.
 */
commander::~commander() throw () {}

/**
 *  Close callback.
 *
 *  @param[in,out] h Handle.
 */
void commander::close(handle& h) {
  if (&h == &_so)
    throw (basic_error() << "received close request on output");
  else {
    logging::error(logging::high) << "received close request on input";
    logging::info(logging::high) << "sending termination request";
    kill(getpid(), SIGTERM);
  }
  return ;
}

/**
 *  Error callback.
 *
 *  @param[in,out] h Handle.
 */
void commander::error(handle& h) {
  if (&h == &_so)
    throw (basic_error() << "received error on output");
  else {
    logging::error(logging::high) << "received error on input";
    logging::info(logging::high) << "sending termination request";
    kill(getpid(), SIGTERM);
  }
  return ;
}

/**
 *  Get class instance.
 *
 *  @return Class instance.
 */
commander& commander::instance() {
  return (*_instance);
}

/**
 *  Load singleton.
 */
void commander::load() {
  _instance.reset(new commander);
  return ;
}

/**
 *  Read callback.
 *
 *  @param[in,out] h Handle.
 */
void commander::read(handle& h) {
  // Read data.
  logging::debug(logging::medium) << "reading data from stdin";
  char buffer[4096];
  unsigned long rb(h.read(buffer, sizeof(buffer)));
  logging::debug(logging::medium) << "read "
    << rb << " bytes from stdin";

  // stdin's eof is reached.
  if (!rb) {
    logging::debug(logging::high) << "got eof on stdin, closing it";
    _si.close();
  }
  // Data was read.
  else {
    _rbuffer.append(buffer, rb);

    // Find a command boundary.
    char boundary[4];
    memset(boundary, 0, sizeof(boundary));
    size_t bound(_rbuffer.find(boundary, 0, sizeof(boundary)));

    // Parse command.
    while (bound != std::string::npos) {
      logging::debug(logging::low)
        << "got command boundary at offset " << bound;
      bound += sizeof(boundary);
      std::string cmd(_rbuffer.substr(0, bound));
      _rbuffer.erase(0, bound);
      _parse(cmd);
      bound = _rbuffer.find(boundary, 0, sizeof(boundary));
    }
  }

  return ;
}

/**
 *  Register commander with multiplexer.
 */
void commander::reg() {
  unreg();
  multiplexer::instance().handle_manager::add(&_si, this);
  multiplexer::instance().handle_manager::add(&_so, this);
  return ;
}

/**
 *  Unload singleton.
 */
void commander::unload() {
  _instance.reset();
  return ;
}

/**
 *  Unregister commander with multiplexer.
 *
 *  @param[in] all Set to true to remove both input and output. Set to
 *                 false to remove only input.
 */
void commander::unreg(bool all) {
  if (all)
    multiplexer::instance().handle_manager::remove(this);
  else
    multiplexer::instance().handle_manager::remove(&_si);
  return ;
}

/**
 *  Do we want to monitor handle for reading ?
 *
 *  @param[in] h Handle.
 */
bool commander::want_read(handle& h) {
  return (&h == &_si);
}

/**
 *  Do we want to monitor handle for writing ?
 *
 *  @param[in] h Handle.
 */
bool commander::want_write(handle& h) {
  return ((&h == &_so) && !_wbuffer.empty());
}

/**
 *  Write callback.
 *
 *  @param[in,out] h Handle.
 */
void commander::write(handle& h) {
  unsigned long wb(h.write(_wbuffer.c_str(), _wbuffer.size()));
  _wbuffer.erase(0, wb);
  return ;
}

/**************************************
*                                     *
*           Private Methods           *
*                                     *
**************************************/

/**
 *  Default constructor.
 */
commander::commander() {}

/**
 *  @brief Copy constructor.
 *
 *  Any call to this constructor will result in a call to abort().
 *
 *  @param[in] c Unused.
 */
commander::commander(commander const& c)
  : com::centreon::handle_listener() {
  (void)c;
  assert(!"commander cannot be copied");
  abort();
}

/**
 *  @brief Assignment operator.
 *
 *  Any call to this method will result in a call to abort().
 *
 *  @param[in] c Unused.
 *
 *  @return This object.
 */
commander& commander::operator=(commander const& c) {
  (void)c;
  assert(!"commander cannot be copied");
  abort();
  return (*this);
}

/**
 *  Parse a command.
 *
 *  @param[in] cmd Command to parse.
 */
void commander::_parse(std::string const& cmd) {
  // Get command ID.
  size_t pos(cmd.find('\0'));
  if (std::string::npos == pos)
    throw (basic_error() << "invalid command received");
  unsigned int id(strtoul(cmd.c_str(), NULL, 10));
  ++pos;

  // Process each command as necessary.
  switch (id) {
   case 0: // Version query.
    // Send version response.
    logging::info(logging::low)
      << "received version request, replying with version 1.0.0";
    {
      // Packet ID.
      std::ostringstream packet;
      packet << "1";
      packet.put('\0');
      // Major.
      packet << "0";
      packet.put('\0');
      // Minor.
      packet << "0";
      for (unsigned int i = 0; i < 4; ++i)
        packet.put('\0');

      // Send packet back to monitoring engine.
      _wbuffer.append(packet.str());
    }
    break ;
   case 2: // Execute query.
    {
      // Find command ID.
      size_t end(cmd.find('\0', pos));
      if (std::string::npos == end)
        throw (basic_error() << "invalid execution request received");
      unsigned long long cmd_id(strtoull(cmd.c_str() + pos, NULL, 10));
      pos = end + 1;
      // Find timeout value.
      end = cmd.find('\0', pos);
      if (std::string::npos == end)
        throw (basic_error() << "invalid execution request received");
      time_t timeout(static_cast<time_t>(strtoull(
        cmd.c_str() + pos,
        NULL,
        10)));
      timeout += time(NULL);
      pos = end + 1;
      // Find start time.
      end = cmd.find('\0', pos);
      if (std::string::npos == end)
        throw (basic_error() << "invalid execution request received");
      pos = end + 1;
      // Find command to execute.
      end = cmd.find('\0', pos);
      if (std::string::npos == end)
        throw (basic_error() << "invalid execution request received");
      std::string cmdline(cmd.substr(pos, end - pos));

      // Find target host.
      pos = 0;
      end = cmdline.find(' ', pos);
      if (std::string::npos == end)
        throw (basic_error() << "invalid execution command");
      std::string host(cmdline.substr(pos, end - pos));
      pos = end + 1;
      // Find user name.
      end = cmdline.find(' ', pos);
      if (std::string::npos == end)
        throw (basic_error() << "invalid execution command");
      std::string user(cmdline.substr(pos, end - pos));
      pos = end + 1;
      // Find password.
      end = cmdline.find(' ', pos);
      if (std::string::npos == end)
        throw (basic_error() << "invalid execution command");
      std::string password(cmdline.substr(pos, end - pos));
      pos = end + 1;
      // Find command.
      std::string command(cmdline.substr(pos));

      logging::info(logging::high)
        << "received command execution request\n"
        << "  command ID  " << cmd_id << "\n"
        << "  timeout     " << timeout << "\n"
        // << "  start time  " << start_time << "\n"
        << "  host        " << host << "\n"
        << "  user        " << user << "\n"
        << "  command     " << command;

      // Run command.
      // credentials cred(host, user, password);
      // session*& sess(sessions::instance()[cred]);
      // if (!sess)
      //   sess = new session(host, user, password);
      // sess->run(command, cmd_id, timeout);
    }
   case 4: // Quit query.
    _si.close();
    break ;
  };

  return ;
}
