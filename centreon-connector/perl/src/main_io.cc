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

#include "com/centreon/connector/perl/main_io.hh"
#include <unistd.h>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <sstream>
#include "com/centreon/connector/perl/embedded.hh"

using namespace com::centreon::connector::perl;

/**************************************
 *                                     *
 *           Private Methods           *
 *                                     *
 **************************************/

/**
 *  Default constructor.
 */
main_io::main_io() {}

/**
 *  Parse a command.
 *
 *  @param[in] cmd Command string.
 *
 *  @return 0 on success.
 */
int main_io::_parse(std::string const& cmd) {
  // Get command ID.
  size_t pos(cmd.find('\0'));
  if (std::string::npos == pos) {
    std::cerr << "invalid command received" << std::endl;
    return (1);
  }
  unsigned int cmd_id(strtoul(cmd.c_str(), NULL, 10));
  ++pos;

  // Process each command as necessary.
  switch (cmd_id) {
    case 0:  // Version query.
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
        main_io::instance().write(packet.str());
      }
      break;
    case 2:  // Execute query.
    {
      // Find command ID.
      size_t end(cmd.find('\0', pos));
      if (std::string::npos == end) {
        std::cerr << "invalid execution request received" << std::endl;
        return (1);
      }
      unsigned long long id(strtoull(cmd.c_str() + pos, NULL, 10));
      pos = end + 1;
      // Find timeout value.
      end = cmd.find('\0', pos);
      if (std::string::npos == end) {
        std::cerr << "invalid execution request received" << std::endl;
        return (1);
      }
      time_t timeout(strtoul(cmd.c_str() + pos, NULL, 10));
      pos = end + 1;
      // Find start time.
      end = cmd.find('\0', pos);
      if (std::string::npos == end) {
        std::cerr << "invalid execution request received" << std::endl;
        return (1);
      }
      pos = end + 1;
      // Find command to execute.
      end = cmd.find('\0', pos);
      if (std::string::npos == end) {
        std::cerr << "invalid execution request received" << std::endl;
        return (1);
      }
      std::string cmdline(cmd.substr(pos, end - pos));

      // Run command.
      embedded::run_command(cmdline, id, timeout);
    } break;
    case 4:  // Quit query.
      return (1);
      break;
  }

  return (0);
}

/**************************************
 *                                     *
 *           Public Methods            *
 *                                     *
 **************************************/

/**
 *  Destructor.
 */
main_io::~main_io() {}

/**
 *  Get the instance of this class.
 *
 *  @return Class instance.
 */
main_io& main_io::instance() {
  static main_io gl;
  return (gl);
}

/**
 *  Read data.
 *
 *  @return 0 on success.
 */
int main_io::read() {
  // Return value.
  int retval(0);

  // Read data.
  char buffer[BUFSIZ];
  ssize_t rb(::read(STDIN_FILENO, buffer, sizeof(buffer)));
  if (rb < 0) {
    char const* msg(strerror(errno));
    std::cerr << "could not read commands from monitoring engine: " << msg
              << std::endl;
    retval = 1;
  } else if (!rb)
    retval = 1;
  else {
    // Append data to buffer.
    _rbuffer.append(buffer, rb);

    // Find a command boundary.
    char boundary[4];
    memset(boundary, 0, sizeof(boundary));
    size_t bound(_rbuffer.find(boundary, 0, sizeof(boundary)));

    // Parse command.
    while (bound != std::string::npos) {
      bound += sizeof(boundary);
      retval = _parse(_rbuffer.substr(0, bound));
      _rbuffer.erase(0, bound);
      bound = _rbuffer.find(boundary, 0, sizeof(boundary));
    }
  }
  return (retval);
}

/**
 *  Write data.
 *
 *  @return 0 on success.
 */
int main_io::write() {
  size_t size(_wbuffer.size());
  ssize_t wb(::write(STDOUT_FILENO, _wbuffer.c_str(), size));
  int retval;
  if (wb < 0) {
    char const* msg(strerror(errno));
    std::cerr << "could not write data to monitoring engine: " << msg
              << std::endl;
    retval = 1;
  } else if (!wb) {
    retval = 1;
  } else {
    _wbuffer.erase(0, wb);
    retval = 0;
  }
  return (retval);
}

/**
 *  Send data to parent process.
 *
 *  @param[in] data Data to send.
 */
void main_io::write(std::string const& data) {
  _wbuffer.append(data);
  return;
}

/**
 *  Check wether or not the multiplexing engine should check for write
 *  readiness on write FD.
 *
 *  @return true.
 */
bool main_io::write_wanted() const {
  return (!_wbuffer.empty());
}
