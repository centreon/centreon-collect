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

#include "com/centreon/benchmark/connector/connector.hh"
#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include <iostream>
#include <sstream>
#include "com/centreon/benchmark/connector/basic_exception.hh"
#include "com/centreon/benchmark/connector/misc.hh"

using namespace com::centreon::benchmark::connector;

/**
 *  Default constructor.
 *
 *  @param[in] commands_file  The path of the commands file.
 *  @param[in] args           Connector command line argument.
 */
connector::connector(std::string const& commands_file,
                     std::list<std::string> const& args)
    : benchmark(),
      _args(args),
      _commands_file(commands_file),
      _current_running(0),
      _pid(0) {
  memset(&_pipe_in, 0, sizeof(_pipe_in));
  memset(&_pipe_out, 0, sizeof(_pipe_out));
  memset(&_pfd, 0, sizeof(_pfd));
}

/**
 *  Default copy constructor.
 *
 *  @param[in] right  The object to copy.
 */
connector::connector(connector const& right) : benchmark() {
  _internal_copy(right);
}

/**
 *  Default destructor.
 */
connector::~connector() throw() {
  _cleanup();
}

/**
 *  Default copy constructor.
 *
 *  @param[in] right  The object to copy.
 *
 *  @return This object.
 */
connector& connector::operator=(connector const& right) {
  return (_internal_copy(right));
}

/**
 *  Execute the benchmark.
 */
void connector::run() {
  _cleanup();

  _commands = load_commands_file(_commands_file);
  _start_connector();

  _check_version();
  _check_execution();
  _check_quit();

  _wait_connector();
}

/**
 *  Send and check the commands execution.
 */
void connector::_check_execution() {
  unsigned int nb_commands(_commands.size());
  for (unsigned int i(0); i < _total_request; ++i) {
    while (_current_running > _limit_running)
      _write(_get_next_result());
    _send_data(_request_execute(i + 1, _commands[i % nb_commands], 1000));
    _recv_data();
  }

  while (_current_running > 0)
    _write(_get_next_result());
}

/**
 *  Send and check quit request.
 */
void connector::_check_quit() {
  _send_data(_request_quit());
}

/**
 *  Send and check version request.
 */
void connector::_check_version() {
  _send_data(_request_version());
  _get_next_result();
}

/**
 *  Clean ressources.
 */
void connector::_cleanup() {
  _wait_connector();
  for (unsigned int i(0); i < 2; ++i) {
    if (_pipe_in[i]) {
      close(_pipe_in[i]);
      _pipe_in[i] = 0;
    }
    if (_pipe_out[i]) {
      close(_pipe_out[i]);
      _pipe_out[i] = 0;
    }
  }
  _commands.clear();
  _results.clear();
  _current_running = 0;
}

/**
 *  Default copy constructor.
 *
 *  @param[in] right  The object to copy.
 *
 *  @return This object.
 */
connector& connector::_internal_copy(connector const& right) {
  (void)right;
  assert(!"impossible to copy connector");
  abort();
  return (*this);
}

/**
 *  Get the next avaialable result.
 *
 *  @return The result.
 */
std::string connector::_get_next_result() {
  static char boundary[] = "\0\0\0\0";
  std::string result;
  size_t pos(0);
  if ((pos = _results.find(boundary, 0, sizeof(boundary) - 1)) !=
      std::string::npos) {
    result = _results.substr(0, pos + sizeof(boundary) - 1);
    _results.erase(0, pos + sizeof(boundary) - 1);
  } else {
    _recv_data(-1);
    return (_get_next_result());
  }
  --_current_running;
  return (result);
}

/**
 *  Get data from connector.
 *
 *  @param[in] timeout  The time to wait data.
 */
void connector::_recv_data(int timeout) {
  int ret(poll(&_pfd, 1, timeout));
  if (ret == -1)
    throw(basic_exception(strerror(errno)));
  else if (ret && (_pfd.revents & (POLLNVAL | POLLHUP)) &&
           !(_pfd.revents & (POLLIN | POLLPRI)))
    throw(
        basic_exception("connector communication fd "
                        "terminated prematurely"));
  else if (!ret || !(_pfd.revents & (POLLIN | POLLPRI)))
    return;
  char buffer[4096];
  if ((ret = read(_pipe_out[0], buffer, sizeof(buffer))) == -1)
    throw(basic_exception(strerror(errno)));
  if (ret)
    _results.append(buffer, ret);
}

/**
 *  Build the request execute.
 *
 *  @param[in] id       The command id.
 *  @param[in] command  The comamnd.
 *  @param[in] timeout  The command timeout.
 *
 *  @return The request string.
 */
std::string connector::_request_execute(unsigned int id,
                                        std::string const& command,
                                        unsigned int timeout) {
  std::stringstream oss;

  oss.write("2\000", 2);
  oss << id;
  oss.write("\000", 1);
  oss << timeout;
  oss.write("\000", 1);
  oss << time(NULL);
  oss.write("\000", 1);
  oss << command;
  oss.write("\000\000\000\000", 4);
  return (oss.str());
}

/**
 *  Build the request quit.
 *
 *  @return The request string.
 */
std::string connector::_request_quit() {
  return (std::string("4\000\000\000\000", 5));
}

/**
 *  Build the request version.
 *
 *  @return The request string.
 */
std::string connector::_request_version() {
  return (std::string("0\000\000\000\000", 5));
}

/**
 *  Send data to the connector.
 *
 *  @param[in] data  The data to send.
 */
void connector::_send_data(std::string const& data) {
  int ret(write(_pipe_in[1], data.c_str(), data.size()));
  if (ret < 0)
    throw(basic_exception(strerror(errno)));
  if (static_cast<unsigned int>(ret) != data.size())
    throw(basic_exception("send data failed"));
  ++_current_running;
}

/**
 *  Start connector.
 */
void connector::_start_connector() {
  if (pipe(_pipe_in) == -1 || pipe(_pipe_out) == -1)
    throw(basic_exception(strerror(errno)));

  _pid = fork();
  if (_pid == -1)
    throw(basic_exception(strerror(errno)));

  if (!_pid) {
    close(_pipe_out[0]);
    close(_pipe_in[1]);
    if (dup2(_pipe_out[1], 1) != -1 && dup2(_pipe_in[0], 0) != -1) {
      close(_pipe_in[0]);
      close(_pipe_out[1]);
      char** arg(list_to_tab(_args));
      execvp(arg[0], arg);
    }
    std::cerr << "error: " << strerror(errno) << std::endl;
    exit(-1);
  }
  close(_pipe_out[1]);
  close(_pipe_in[0]);

  _pfd.fd = _pipe_out[0];
  _pfd.events = POLLIN | POLLPRI;
}

/**
 *  Wait the end of the connector.
 */
void connector::_wait_connector() {
  if (_pid) {
    int status;
    waitpid(_pid, &status, 0);
    _pid = 0;
  }
}
