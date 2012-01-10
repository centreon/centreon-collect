/*
** Copyright 2011 Merethis
**
** This file is part of Centreon Connector ICMP.
**
** Centreon Connector ICMP is free software: you can redistribute it
** and/or modify it under the terms of the GNU Affero General Public
** License as published by the Free Software Foundation, either version
** 3 of the License, or (at your option) any later version.
**
** Centreon Connector ICMP is distributed in the hope that it will be
** useful, but WITHOUT ANY WARRANTY; without even the implied warranty
** of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
** Affero General Public License for more details.
**
** You should have received a copy of the GNU Affero General Public
** License along with Centreon Connector ICMP. If not, see
** <http://www.gnu.org/licenses/>.
*/

#include <iostream>
#include "com/centreon/exceptions/basic.hh"
#include "com/centreon/connector/icmp/request.hh"

using namespace com::centreon::connector::icmp;

/**
 *  Build the request execute.
 *
 *  @param[in] id       The command id.
 *  @param[in] command  The comamnd.
 *  @param[in] timeout  The command timeout.
 *
 *  @return The request string.
 */
static std::string execute(
                     unsigned int id,
                     std::string const& command,
                     unsigned int timeout) {
  std::stringstream oss;

  oss.write("2\000", 2);
  oss << id; oss.write("\000", 1);
  oss << timeout; oss.write("\000", 1);
  oss << time(NULL); oss.write("\000", 1);
  oss << command; oss.write("\000\000\000\000", 4);
  return (oss.str());
}

/**
 *  Check request execute.
 *
 *  @return 0 on success.
 */
int main() {
  try {
    request req(execute(42, "test", 10));
    if (req.id() != request::execute)
      throw (basic_error() << "invalid request id");
    unsigned int id;
    req.next_argument(id);
    if (id != 42)
      throw (basic_error() << "invalid command id");
    unsigned int timeout;
    req.next_argument(timeout);
    if (timeout != 10)
      throw (basic_error() << "invalid command timeout");
    unsigned int start_time;
    req.next_argument(start_time);
    if (!start_time)
      throw (basic_error() << "invalid command start time");
    std::string cmd;
    req.next_argument(cmd);
    if (cmd != "test")
      throw (basic_error() << "invalid command");
  }
  catch (std::exception const& e) {
    std::cerr << "error: " << e.what() << std::endl;
    return (1);
  }
  return (0);
}
