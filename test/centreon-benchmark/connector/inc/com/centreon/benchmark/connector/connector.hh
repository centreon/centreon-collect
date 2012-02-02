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

#ifndef CCB_CONNECTOR_CONNECTOR
#  define CCB_CONNECTOR_CONNECTOR

#  include <list>
#  include <poll.h>
#  include <string>
#  include <sys/types.h>
#  include <vector>
#  include "com/centreon/benchmark/connector/benchmark.hh"
#  include "com/centreon/benchmark/connector/namespace.hh"

CCB_CONNECTOR_BEGIN()

/**
 *  @class connector connector.hh "com/centreon/benchmark/connector/connector.hh"
 *  @brief Implementation of benchmark for testing connector.
 *
 *  This class is an implementation of benchmark for testing connector.
 */
class                      connector : public benchmark {
public:
                           connector(
                             std::string const& commands_file,
                             std::list<std::string> const& args);
                           connector(connector const& right);
                           ~connector() throw ();
  connector&               operator=(connector const& right);

  void                     run();

private:
  void                     _check_execution();
  void                     _check_quit();
  void                     _check_version();
  void                     _cleanup();
  connector&               _internal_copy(connector const& right);
  std::string              _get_next_result();
  void                     _recv_data(int timeout = 0);
  static std::string       _request_execute(
                             unsigned int id,
                             std::string const& command,
                             unsigned int timeout);
  static std::string       _request_quit();
  static std::string       _request_version();
  void                     _send_data(std::string const& data);
  void                     _start_connector();
  void                     _wait_connector();

  std::list<std::string>   _args;
  std::vector<std::string> _commands;
  std::string              _commands_file;
  unsigned int             _current_running;
  int                      _pipe_in[2];
  int                      _pipe_out[2];
  pid_t                    _pid;
  pollfd                   _pfd;
  std::string              _results;
};

CCB_CONNECTOR_END()

#endif // !CCB_CONNECTOR_CONNECTOR
