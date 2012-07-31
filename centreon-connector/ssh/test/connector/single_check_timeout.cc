/*
** Copyright 2012 Merethis
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

#include <sstream>
#include <string>
#include <string.h>
#include <time.h>
#include "com/centreon/clib.hh"
#include "com/centreon/concurrency/thread.hh"
#include "com/centreon/process.hh"
#include "test/connector/binary.hh"

using namespace com::centreon;

#define CMD1 "2\0" \
             "4242\0" \
             "3\0" \
             "123456789\0" \
             "check_by_ssh " \
             "-H localhost " \
             " -C 'sleep 30'\0\0\0\0"
#define RESULT "3\0" \
               "4242\0" \
               "0\0" \
               "-1\0" \
               " \0" \
               " \0\0\0\0"

/**
 *  The process killing task.
 */
class      kill_connector : public concurrency::thread {
public:
  /**
   *  Constructor.
   */
           kill_connector(process* p, time_t when)
    : _p(p), _when(when) {}

  /**
   *  Run thread.
   */
  void     _run() {
    time_t now(time(NULL));
    if (now < _when)
      sleep(_when - now);
    _p->terminate();
    return ;
  }

private:
  process* _p;
  time_t   _when;
};

/**
 *  Check that connector set timeout on commands.
 *
 *  @return 0 on success.
 */
int main() {
  clib::load();
  // Process.
  process p;
  p.enable_stream(process::in, true);
  p.enable_stream(process::out, true);
  p.exec(CONNECTOR_SSH_BINARY);

  // Write command.
  std::ostringstream oss;
  oss.write(CMD1, sizeof(CMD1) - 1);
  std::string cmd(oss.str());
  char const* ptr(cmd.c_str());
  unsigned int size(cmd.size());
  while (size > 0) {
    unsigned int rb(p.write(ptr, size));
    size -= rb;
    ptr += rb;
  }
  p.enable_stream(process::in, false);

  // Schedule process termination.
  kill_connector killer(&p, time(NULL) + 4);
  killer.exec();

  // Read reply.
  std::string output;
  while (true) {
    std::string buffer;
    p.read(buffer);
    if (buffer.empty())
      break;
    output.append(buffer);
  }

  // Wait for process termination.
  p.wait();
  int retval(p.exit_code() != 0);

  clib::unload();

  // Compare results.
  return (retval
          || (output.size() != (sizeof(RESULT) - 1))
          || (memcmp(output.c_str(), RESULT, sizeof(RESULT) - 1)));;
}
