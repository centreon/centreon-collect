/*
** Copyright 2026 Centreon
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

#include "com/centreon/connector/perl/check_child.hh"

#include <EXTERN.h>
#include <perl.h>

#include "src/perl_connector.pb.h"

using namespace com::centreon::connector::perl;

/************************************************************************
 *    parent side
 *************************************************************************/

void check_child::_on_stdout_read(const std::string received) {
  std::vector<ConnectorMess> decoded;

  auto forward_to_handler = [&, this]() {
    for (const ConnectorMess& to_read : decoded) {
      if (to_read.has_result()) {
        _running = false;
      }
      _parent_read_handler(get_pid(), to_read);
    }
  };

  try {
    _protocol.on_recv(received, decoded);
    forward_to_handler();
  } catch (const std::exception& e) {
    forward_to_handler();
    SPDLOG_LOGGER_ERROR(_logger,
                        "fail to decode datas from child pid={}: {} => kill",
                        get_pid(), e.what());
    kill();
  }
}

void check_child::_on_process_end() {
  _parent_end_child_handler(get_pid());
}

/************************************************************************
 *    child side
 *************************************************************************/

check_child::load check_child::measure_load() {}

int check_child::_run(int stdin_fd, int stdout_fd, int) {
  asio::readable_pipe child_stdin(*_io_context, stdin_fd);
  asio::writable_pipe child_stdout(*_io_context, stdout_fd);

  while (1) {
    ConnectorMess received;
    boost::system::error_code err = _protocol.recv(child_stdin, received);
    if (err) {
      break;
    }
    if (received.has_terminate()) {
      break;
    }
    if (received.has_execute()) {
    }
  }

  return 0;
}
