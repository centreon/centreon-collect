/*
** Copyright 2011 Merethis
**
** This file is part of Centreon Plugin ICMP.
**
** Centreon Plugin ICMP is free software: you can redistribute it
** and/or modify it under the terms of the GNU Affero General Public
** License as published by the Free Software Foundation, either version
** 3 of the License, or (at your option) any later version.
**
** Centreon Plugin ICMP is distributed in the hope that it will be
** useful, but WITHOUT ANY WARRANTY; without even the implied warranty
** of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
** Affero General Public License for more details.
**
** You should have received a copy of the GNU Affero General Public
** License along with Centreon Plugin ICMP. If not, see
** <http://www.gnu.org/licenses/>.
*/

#ifndef CCB_CONNECTOR_PLUGIN
#  define CCB_CONNECTOR_PLUGIN

#  include <map>
#  include <string>
#  include <sys/types.h>
#  include <vector>
#  include "com/centreon/benchmark/connector/benchmark.hh"
#  include "com/centreon/benchmark/connector/namespace.hh"

CCB_CONNECTOR_BEGIN()

/**
 *  @class plugin plugin.hh "com/centreon/benchmark/connector/plugin.hh"
 *  @brief Implementation of benchmark for testing nagios plugin.
 *
 *  This class is an implementation of benchmark for testing nagios
 *  plugin.
 */
class                      plugin : public benchmark {
public:
                           plugin(
                             std::string const& commands_file,
                             std::vector<std::string> const& args);
                           plugin(plugin const& right);
                           ~plugin() throw ();
  plugin&                  operator=(plugin const& right);

  void                     run();

private:
  void                     _cleanup();
  plugin&                  _internal_copy(plugin const& right);
  void                     _recv_data(int fd);
  void                     _start_plugin(char** args);
  void                     _wait_plugin(bool block);

  std::vector<std::string> _args;
  std::vector<std::string> _commands;
  std::string              _commands_file;
  unsigned int             _current_running;
  std::map<pid_t, int>     _pid;
};

CCB_CONNECTOR_END()

#endif // !CCB_CONNECTOR_PLUGIN
