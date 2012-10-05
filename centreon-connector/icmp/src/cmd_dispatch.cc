/*
** Copyright 2011-2012 Merethis
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
#include <stdio.h>
#include <stdlib.h>
#include "com/centreon/concurrency/locker.hh"
#include "com/centreon/connector/icmp/result.hh"
#include "com/centreon/connector/icmp/version.hh"
#include "com/centreon/logging/logger.hh"
#include "com/centreon/connector/icmp/cmd_dispatch.hh"

using namespace com::centreon::concurrency;
using namespace com::centreon::connector::icmp;

/**
 *  Default constructor.
 */
cmd_dispatch::cmd_dispatch(unsigned int max_concurrent_checks)
  : thread(),
    handle_listener(),
    check_observer(),
    _check_dispatcher(this),
    _current_execution(0),
    _input(stdin),
    _output(stdout),
    _quit(false),
    _t_manager(1),
    _h_manager(&_t_manager) {
  _h_manager.add(&_input, this);
  _h_manager.add(&_output, this);
  _h_manager.add(&_interrupt, &_interrupt);
  _check_dispatcher.set_max_concurrent_checks(max_concurrent_checks);
}

/**
 *  Default destructor.
 */
cmd_dispatch::~cmd_dispatch() throw () {

}

/**
 *  Default copy constructor.
 *
 *  @param[in] right  The object to copy.
 */
cmd_dispatch::cmd_dispatch(cmd_dispatch const& right)
  : thread(),
    handle_listener(),
    check_observer() {
  _internal_copy(right);
}

/**
 *  Default copy operator.
 *
 *  @param[in] right  The object to copy.
 *
 *  @return This object.
 */
cmd_dispatch& cmd_dispatch::operator=(cmd_dispatch const& right) {
  return (_internal_copy(right));
}

/**
 *  Ask thread to quit.
 */
void cmd_dispatch::exit() {
  _quit = true;
}

/**
 *  Close event for a specific handle.
 *
 *  @param[in] h  The handle affected by the event.
 */
void cmd_dispatch::close(handle& h) {
  (void)h;
  _quit = true;
  log_debug(logging::low) << "the standard output was close";
}

/**
 *  Recv result of one check.
 *
 *  @param[in] command_id  The connector command id.
 *  @param[in] status      Status of check result.
 *  @param[in] msg         Message of check result.
 */
void cmd_dispatch::emit_check_result(
                     unsigned int command_id,
                     unsigned int status,
                     std::string const& msg) {
  result res(result::execute);
  res << command_id  // The connector command id.
      << true        // The command was execute.
      << status      // The connector exit code.
      << ""          // The connector exit message error.
      << msg;        // The connector exit message.
  locker lock(&_mtx);
  _results.push_back(res.data());
  --_current_execution;
  _interrupt.wake();
}

/**
 *  Error event for a specific handle.
 *
 *  @param[in] h  The handle affected by the event.
 */
void cmd_dispatch::error(handle& h) {
  (void)h;
  _quit = true;
  log_debug(logging::low) << "the standard output had an error";
}

/**
 *  Read event for a specific handle.
 *
 *  @param[in] h  The handle affected by the event.
 */
void cmd_dispatch::read(handle& h) {
  static char boundary[] = "\0\0\0\0";

  try {
    char buffer[4096];
    unsigned long size(h.read(buffer, sizeof(buffer)));
    if (!size) {
      _quit = true;
      return;
    }

    _buffer.append(buffer, size);

    size_t pos(0);
    while ((pos = _buffer.find(boundary, 0, sizeof(boundary) - 1))
           != std::string::npos) {
      try {
        _requests.push_back(_buffer.substr(0, pos));
      }
      catch (std::exception const& e) {
        log_error(logging::low) << e.what();
      }
      _buffer.erase(0, pos + sizeof(boundary) - 1);
    }
  }
  catch (std::exception const& e) {
    log_error(logging::low) << e.what();
  }
}

/**
 *  This methode was notify for read event.
 *
 *  @param[in] h  The handle we want to get read event.
 *
 *  @return True if handle is standard input, otherwise false.
 */
bool cmd_dispatch::want_read(handle& h) {
  return (&h == &_input);
}

/**
 *  This methode was notify for write event.
 *
 *  @param[in] h  The handle we want to get write event.
 *
 *  @return True if handle is standard output, otherwise false.
 */
bool cmd_dispatch::want_write(handle& h) {
  locker lock(&_mtx);
  return (&h == &_output && !_results.empty());
}

/**
 *  Write event for a specific handle.
 *
 *  @param[in] h  The handle affected by the event.
 */
void cmd_dispatch::write(handle& h) {
  try {
    locker lock(&_mtx);
    std::string& res(_results.front());
    unsigned long size(h.write(res.c_str(), res.size()));
    if (size == res.size())
      _results.pop_front();
    else
      res.erase(0, size);
  }
  catch (std::exception const& e) {
    log_error(logging::low) << e.what();
  }
}

/**
 *  Internal copy.
 *
 *  @param[in] right  The object to copy.
 *
 *  @return This object.
 */
cmd_dispatch& cmd_dispatch::_internal_copy(cmd_dispatch const& right) {
  (void)right;
  assert(!"impossible to copy cmd_dispatch");
  abort();
  return (*this);
}

/**
 *  Process all request.
 */
void cmd_dispatch::_request_processing() {
  while (!_requests.empty()) {
    request& req(_requests.front());
    switch (req.id()) {
    case request::version: {
      log_debug(logging::low) << "receive request::version";

      result res(result::version);
      res << version::get_engine_major()
          << version::get_engine_minor();
      locker lock(&_mtx);
      _results.push_back(res.data());
      break;
    }

    case request::execute: {
      unsigned long command_id(0);
      unsigned int timeout(0);
      unsigned int start_time(0);
      std::string command;
      if (req.next_argument(command_id)
          && req.next_argument(timeout)
          && req.next_argument(start_time)
          && req.next_argument(command)) {
        log_debug(logging::low)
          << "receive request::execute (" << command_id << ", "
          << timeout << ", " << start_time << ", " << command << ")";

        _check_dispatcher.submit(command_id, command);
        locker lock(&_mtx);
        ++_current_execution;
      }
      else {
        log_debug(logging::low)
          << "receive request::execute (invalid request)";

        result res(result::error);
        res << 2 << "invalid request";
        locker lock(&_mtx);
        _results.push_back(res.data());
      }
      break;
    }

    case request::quit: {
      log_debug(logging::low) << "receive request::quit";

      result res(result::quit);
      locker lock(&_mtx);
      _results.push_back(res.data());
      _h_manager.remove(&_input);
      _quit = true;
      break;
    }
    }
    _requests.pop_front();
  }
}

/**
 *  Main loop of command dispatcher.
 */
void cmd_dispatch::_run() {
  try {
    while (true) {
      _h_manager.multiplex();
      _request_processing();

      if (_quit) {
        locker lock(&_mtx);
        if (!_current_execution && _results.empty())
          break;
      }
    }
  }
  catch (std::exception const& e) {
    log_error(logging::low) << e.what();
  }
}
