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
#include <errno.h>
#include <sstream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "com/centreon/connector/ssh/credentials.hh"
#include "com/centreon/connector/ssh/exception.hh"
#include "com/centreon/connector/ssh/session.hh"
#include "com/centreon/connector/ssh/sessions.hh"
#include "com/centreon/connector/ssh/std_io.hh"

using namespace com::centreon::connector::ssh;

/**************************************
*                                     *
*           Private Methods           *
*                                     *
**************************************/

/**
 *  Constructor.
 */
std_io::std_io() {}

/**
 *  @brief Copy constructor.
 *
 *  Any call to this constructor will result in a call to abort().
 *
 *  @param[in] sio Object to copy.
 */
std_io::std_io(std_io const& sio) {
  (void)sio;
  assert(false);
  abort();
}

/**
 *  @brief Assignment operator.
 *
 *  Any call to this method will result in a call to abort().
 *
 *  @param[in] sio Object to copy.
 *
 *  @return This object.
 */
std_io& std_io::operator=(std_io const& sio) {
  (void)sio;
  assert(false);
  abort();
  return (*this);
}

/**
 *  Parse a command.
 *
 *  @param[in] cmd Command to parse.
 */
void std_io::_parse(std::string const& cmd) {
  // Get command ID.
  size_t pos(cmd.find('\0'));
  if (std::string::npos == pos)
    throw (exception() << "invalid command received");
  unsigned int id(strtoul(cmd.c_str(), NULL, 10));
  ++pos;

  // Process each command as necessary.
  switch (id) {
   case 0: // Version query.
    // Send version response.
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
        throw (exception() << "invalid execution request received");
      unsigned long long cmd_id(strtoull(cmd.c_str() + pos, NULL, 10));
      pos = end + 1;
      // Find timeout value.
      end = cmd.find('\0', pos);
      if (std::string::npos == end)
        throw (exception() << "invalid execution request received");
      time_t timeout(static_cast<time_t>(strtoull(cmd.c_str() + pos,
        NULL,
        10)));
      timeout += time(NULL);
      pos = end + 1;
      // Find start time.
      end = cmd.find('\0', pos);
      if (std::string::npos == end)
        throw (exception() << "invalid execution request received");
      pos = end + 1;
      // Find command to execute.
      end = cmd.find('\0', pos);
      if (std::string::npos == end)
        throw (exception() << "invalid execution request received");
      std::string cmdline(cmd.substr(pos, end - pos));

      // Find target host.
      pos = 0;
      end = cmdline.find(' ', pos);
      if (std::string::npos == end)
        throw (exception() << "invalid execution command");
      std::string host(cmdline.substr(pos, end - pos));
      pos = end + 1;
      // Find user name.
      end = cmdline.find(' ', pos);
      if (std::string::npos == end)
        throw (exception() << "invalid execution command");
      std::string user(cmdline.substr(pos, end - pos));
      pos = end + 1;
      // Find password.
      end = cmdline.find(' ', pos);
      if (std::string::npos == end)
        throw (exception() << "invalid execution command");
      std::string password(cmdline.substr(pos, end - pos));
      pos = end + 1;
      // Find command.
      std::string command(cmdline.substr(pos));

      // Run command.
      credentials cred(host, user, password);
      session*& sess(sessions::instance()[cred]);
      if (!sess)
        sess = new session(host, user, password);
      sess->run(command, cmd_id, timeout);
    }
   case 4: // Quit query.
    ::close(STDIN_FILENO);
    break ;
  };

  return ;
}

/**************************************
*                                     *
*           Public Methods            *
*                                     *
**************************************/

/**
 *  Destructor.
 */
std_io::~std_io() {}

/**
 *  Get the class instance.
 *
 *  @return Class instance.
 */
std_io& std_io::instance() {
  static std_io gl;
  return (gl);
}

/**
 *  Read some data on the standard input.
 *
 *  @return true while data can be read from stdin.
 */
bool std_io::read() {
  // Read data.
  char buffer[BUFSIZ];
  ssize_t rb(::read(STDIN_FILENO, buffer, sizeof(buffer)));
  // An error occurred.
  if (rb < 0) {
    char const* msg(strerror(errno));
    throw (exception() << "could not read from standard input: "
             << msg);
  }

  // Append data to buffer.
  _rbuffer.append(buffer, rb);

  // Find a command boundary.
  char boundary[4];
  memset(boundary, 0, sizeof(boundary));
  size_t bound(_rbuffer.find(boundary, 0, sizeof(boundary)));

  // Parse command.
  while (bound != std::string::npos) {
    bound += sizeof(boundary);
    std::string cmd(_rbuffer.substr(0, bound));
    _rbuffer.erase(0, bound);
    _parse(cmd);
    bound = _rbuffer.find(boundary, 0, sizeof(boundary));
  }

  return (rb != 0);
}

/**
 *  Submit a check result.
 *
 *  @param[in] cmd_id   Command ID.
 *  @param[in] executed true if command was executed, false otherwise.
 *  @param[in] exitcode Command exit code.
 *  @param[in] err      Process' stderr.
 *  @param[in] out      Process' stdout.
 */
void std_io::submit_check_result(unsigned long long cmd_id,
                                 bool executed,
                                 int exitcode,
                                 std::string const& err,
                                 std::string const& out) {
  // Build packet.
  std::ostringstream oss;
  // Packet ID.
  oss << "3";
  oss.put('\0');
  // Command ID.
  oss << cmd_id;
  oss.put('\0');
  // Executed.
  oss << (executed ? "1" : "0");
  oss.put('\0');
  // Exit code.
  oss << exitcode;
  oss.put('\0');
  // Error output.
  if (err.empty())
    oss.put(' ');
  else
    oss << err;
  oss.put('\0');
  // Standard output.
  if (out.empty())
    oss.put(' ');
  else
    oss << out;
  // Packet boundary.
  for (unsigned int i = 0; i < 4; ++i)
    oss.put('\0');
  // Append packet to write buffer.
  _wbuffer.append(oss.str());
  return ;
}

/**
 *  Write data to stdout.
 */
void std_io::write() {
  unsigned int size(_wbuffer.size());
  ssize_t wb(::write(STDOUT_FILENO, _wbuffer.c_str(), size));
  if (wb < 0) {
    char const* msg(strerror(errno));
    throw (exception() << "failure while writing to standard output: "
             << msg);
  }
  else if (!wb)
    throw (exception() << "standard output is closed");
  _wbuffer.erase(0, wb);
  return ;
}

/**
 *  Should we monitor for write availability on stdout ?
 *
 *  @return true if we should.
 */
bool std_io::write_wanted() const {
  return (!_wbuffer.empty());
}
