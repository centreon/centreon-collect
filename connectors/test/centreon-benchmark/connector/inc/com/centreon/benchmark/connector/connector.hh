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

#ifndef CCB_CONNECTOR_CONNECTOR
#define CCB_CONNECTOR_CONNECTOR

#include <poll.h>
#include <sys/types.h>
#include <list>
#include <string>
#include <vector>
#include "com/centreon/benchmark/connector/benchmark.hh"

CCB_CONNECTOR_BEGIN()

/**
 *  @class connector connector.hh
 *"com/centreon/benchmark/connector/connector.hh"
 *  @brief Implementation of benchmark for testing connector.
 *
 *  This class is an implementation of benchmark for testing connector.
 */
class connector : public benchmark {
 public:
  connector(std::string const& commands_file,
            std::list<std::string> const& args);
  connector(connector const& right);
  ~connector() throw();
  connector& operator=(connector const& right);

  void run();

 private:
  void _check_execution();
  void _check_quit();
  void _check_version();
  void _cleanup();
  connector& _internal_copy(connector const& right);
  std::string _get_next_result();
  void _recv_data(int timeout = 0);
  static std::string _request_execute(unsigned int id,
                                      std::string const& command,
                                      unsigned int timeout);
  static std::string _request_quit();
  static std::string _request_version();
  void _send_data(std::string const& data);
  void _start_connector();
  void _wait_connector();

  std::list<std::string> _args;
  std::vector<std::string> _commands;
  std::string _commands_file;
  unsigned int _current_running;
  int _pipe_in[2];
  int _pipe_out[2];
  pid_t _pid;
  pollfd _pfd;
  std::string _results;
};

CCB_CONNECTOR_END()

#endif  // !CCB_CONNECTOR_CONNECTOR
