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

#ifndef CCB_CONNECTOR_PLUGIN
#define CCB_CONNECTOR_PLUGIN

#include <sys/types.h>
#include <list>
#include <map>
#include <string>
#include <vector>
#include "com/centreon/benchmark/connector/benchmark.hh"

CCB_CONNECTOR_BEGIN()

/**
 *  @class plugin plugin.hh "com/centreon/benchmark/connector/plugin.hh"
 *  @brief Implementation of benchmark for testing nagios plugin.
 *
 *  This class is an implementation of benchmark for testing nagios
 *  plugin.
 */
class plugin : public benchmark {
 public:
  plugin(std::string const& commands_file, std::list<std::string> const& args);
  plugin(plugin const& right);
  ~plugin() throw();
  plugin& operator=(plugin const& right);

  void run();

 private:
  void _cleanup();
  plugin& _internal_copy(plugin const& right);
  void _recv_data(int fd);
  void _start_plugin(char** args);
  void _wait_plugin(bool block);

  std::list<std::string> _args;
  std::vector<std::string> _commands;
  std::string _commands_file;
  unsigned int _current_running;
  std::map<pid_t, int> _pid;
};

CCB_CONNECTOR_END()

#endif  // !CCB_CONNECTOR_PLUGIN
