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

#include <assert.h>
#include <errno.h>
#include <iostream>
#include <sstream>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include "com/centreon/benchmark/connector/basic_exception.hh"
#include "com/centreon/benchmark/connector/misc.hh"
#include "com/centreon/benchmark/connector/plugin.hh"

using namespace com::centreon::benchmark::connector;

/**
 *  Default constructor.
 *
 *  @param[in] commands_file  The path of the commands file.
 *  @param[in] args           Plugin command line argument.
 */
plugin::plugin(
          std::string const& commands_file,
          std::vector<std::string> const& args)
  : benchmark(),
    _args(args),
    _commands_file(commands_file),
    _current_running(0) {

}

/**
 *  Default copy constructor.
 *
 *  @param[in] right  The object to copy.
 */
plugin::plugin(plugin const& right)
  : benchmark() {
  _internal_copy(right);
}

/**
 *  Default destructor.
 */
plugin::~plugin() throw () {
  _cleanup();
}

/**
 *  Default copy constructor.
 *
 *  @param[in] right  The object to copy.
 *
 *  @return This object.
 */
plugin& plugin::operator=(plugin const& right) {
  return (_internal_copy(right));
}

/**
 *  Execute the benchmark.
 */
void plugin::run() {
  _cleanup();
  _commands = load_commands_file(_commands_file);

  char** args = com::centreon::benchmark::connector::vector_to_tab(_args, _args.size() + 2);
  try {
    unsigned int pos(_args.size());
    unsigned int nb_commands(_commands.size());
    for (unsigned int i(0); i < _total_request; ++i) {
      args[pos] = const_cast<char*>(_commands[i % nb_commands].c_str());
      if (_current_running > _limit_running)
        _wait_plugin(true);
      _start_plugin(args);
      _wait_plugin(false);
    }

    while (_current_running > 0) {
      _wait_plugin(true);
    }
  }
  catch (std::exception const& e) {
    delete args;
    throw;
  }
  delete args;
}

/**
 *  Clean ressources.
 */
void plugin::_cleanup() {
  int status;
  while (_current_running) {
    pid_t pid(waitpid(-1, &status, 0));
    if (pid == -1)
      throw (basic_exception(strerror(errno)));
    if (pid)
      --_current_running;
  }

  _commands.clear();
  _current_running = 0;
  _pid.clear();
}

/**
 *  Default copy constructor.
 *
 *  @param[in] right  The object to copy.
 *
 *  @return This object.
 */
plugin& plugin::_internal_copy(plugin const& right) {
  (void)right;
  assert(!"impossible to copy plugin");
  abort();
  return (*this);
}

/**
 *  Get data from plugin.
 *
 *  @param[in] fd  The file descriptor to read.
 */
void plugin::_recv_data(int fd) {
  char buffer[4096];
  int ret(read(fd, buffer, sizeof(buffer)));
  if (ret == -1)
    throw (basic_exception(strerror(errno)));
  if (ret)
    _write(buffer, ret);
}

/**
 *  Execute plugin.
 *
 *  @param[in] args  The command line arguments.
 */
void plugin::_start_plugin(char** args) {
  int pipe_out[2];
  if (pipe(pipe_out) == -1)
    throw (basic_exception(strerror(errno)));

  pid_t pid(fork());
  if (pid == -1)
    throw (basic_exception(strerror(errno)));

  if (!pid) {
    close(pipe_out[0]);
    close(2);
    if (dup2(pipe_out[1], 1) != -1) {
      close(pipe_out[1]);
      execvp(args[0], args);
    }
    std::cout << "error: " << strerror(errno) << std::endl;
    exit(-1);
  }
  ++_current_running;
  close(pipe_out[1]);
  _pid[pid] = pipe_out[0];
}

/**
 *  Wait plugin and get data.
 *
 *  @param[in] block  If true block until one process finish.
 */
void plugin::_wait_plugin(bool block) {
  int status;
  pid_t pid(waitpid(-1, &status, block ? 0 : WNOHANG));
  if (pid == -1)
    throw (basic_exception(strerror(errno)));
  if (!pid)
    return;
  std::map<pid_t, int>::iterator it(_pid.find(pid));
  if (it == _pid.end())
    throw (basic_exception("pid not found"));
  _recv_data(it->second);
  close(it->second);
  _pid.erase(it);
  --_current_running;
}
