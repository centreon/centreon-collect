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

#ifndef CCC_ICMP_CMD_DISPATCH_HH
#  define CCC_ICMP_CMD_DISPATCH_HH

#  include <list>
#  include <list>
#  include <string>
#  include "com/centreon/concurrency/mutex.hh"
#  include "com/centreon/concurrency/thread.hh"
#  include "com/centreon/connector/icmp/check_dispatch.hh"
#  include "com/centreon/connector/icmp/check_observer.hh"
#  include "com/centreon/connector/icmp/interrupt.hh"
#  include "com/centreon/connector/icmp/namespace.hh"
#  include "com/centreon/connector/icmp/request.hh"
#  include "com/centreon/io/file_stream.hh"
#  include "com/centreon/handle_listener.hh"
#  include "com/centreon/handle_manager.hh"
#  include "com/centreon/task_manager.hh"

CCC_ICMP_BEGIN()

/**
 *  @class cmd_dispatch cmd_dispatch.hh "com/centreon/connector/icmp/cmd_dispatch.hh"
 *  @brief Cmd_Dispatch request from Centreon Engine.
 *
 *  Dispatch request from Centreon Engine and start execute command.
 */
class                    cmd_dispatch
  : public concurrency::thread,
    private handle_listener, check_observer {
public:
                         cmd_dispatch(unsigned int max_concurrent_checks);
                         ~cmd_dispatch() throw ();
  void                   exit();

private:
                         cmd_dispatch(cmd_dispatch const& right);
  cmd_dispatch&          operator=(cmd_dispatch const& right);
  void                   close(handle& h);
  void                   emit_check_result(
                           unsigned int command_id,
                           unsigned int status,
                           std::string const& msg);
  void                   error(handle& h);
  void                   read(handle& h);
  bool                   want_read(handle& h);
  bool                   want_write(handle& h);
  void                   write(handle& h);
  cmd_dispatch&          _internal_copy(cmd_dispatch const& right);
  void                   _request_processing();
  void                   _run();

  std::string            _buffer;
  check_dispatch         _check_dispatcher;
  unsigned int           _current_execution;
  io::file_stream        _input;
  interrupt              _interrupt;
  concurrency::mutex     _mtx;
  io::file_stream        _output;
  bool                   _quit;
  std::list<request>     _requests;
  std::list<std::string> _results;
  task_manager           _t_manager;
  handle_manager         _h_manager;
};

CCC_ICMP_END()

#endif // !CCC_ICMP_CMD_DISPATCH_HH
