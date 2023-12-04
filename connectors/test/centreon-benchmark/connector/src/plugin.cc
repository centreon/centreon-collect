/**
* Copyright 2011-2013 Centreon
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
*     http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*
* For more information : contact@centreon.com
*/

#include "com/centreon/benchmark/connector/plugin.hh"
#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include <wordexp.h>
#include <iostream>
#include <sstream>
#include "com/centreon/benchmark/connector/basic_exception.hh"
#include "com/centreon/benchmark/connector/misc.hh"

using namespace com::centreon::benchmark::connector;

/**
 *  Default constructor.
 *
 *  @param[in] commands_file  The path of the commands file.
 *  @param[in] args           Plugin command line argument.
 */
plugin::plugin(std::string const& commands_file,
               std::list<std::string> const& args)
    : benchmark(),
      _args(args),
      _commands_file(commands_file),
      _current_running(0) {}

/**
 *  Default copy constructor.
 *
 *  @param[in] right  The object to copy.
 */
plugin::plugin(plugin const& right) : benchmark() {
  _internal_copy(right);
}

/**
 *  Default destructor.
 */
plugin::~plugin() throw() {
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

  wordexp_t p;
  try {
    unsigned int nb_commands(_commands.size());
    for (unsigned int i(0); i < _total_request; ++i) {
      while (_current_running > _limit_running)
        _wait_plugin(true);
      if (wordexp(_commands[i % nb_commands].c_str(), &p, 0))
        throw(basic_exception("parsing argument failed"));
      _start_plugin(p.we_wordv);
      wordfree(&p);
      _wait_plugin(false);
    }

    while (_current_running > 0) {
      _wait_plugin(true);
    }
  } catch (std::exception const& e) {
    wordfree(&p);
    throw;
  }
}

/**
 *  Clean ressources.
 */
void plugin::_cleanup() {
  int status;
  while (_current_running) {
    pid_t pid(waitpid(-1, &status, 0));
    if (pid == -1)
      throw(basic_exception(strerror(errno)));
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
    throw(basic_exception(strerror(errno)));
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
    throw(basic_exception(strerror(errno)));

  pid_t pid(fork());
  if (pid == -1)
    throw(basic_exception(strerror(errno)));

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
    throw(basic_exception(strerror(errno)));
  if (!pid)
    return;
  std::map<pid_t, int>::iterator it(_pid.find(pid));
  if (it == _pid.end())
    throw(basic_exception("pid not found"));
  _recv_data(it->second);
  close(it->second);
  _pid.erase(it);
  --_current_running;
}
