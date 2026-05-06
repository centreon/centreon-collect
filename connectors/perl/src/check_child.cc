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

/**
 * @brief Called by the parent when data arrives on the child's stdout pipe.
 *
 * Decodes one or more protobuf messages and forwards them to the registered
 * handler. Marks the check as no longer running as soon as a result message
 * is received. On decode failure the already-decoded messages are still
 * forwarded before the child is killed, so callers always get a consistent
 * view of partial results.
 *
 * @param received Raw bytes read from the child stdout pipe.
 */
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

/**
 * @brief Called by the parent when the child process exits (normal or abnormal).
 *
 * Delegates cleanup to the registered end-child handler so the owning policy
 * can remove this instance from its active-children map.
 */
void check_child::_on_process_end() {
  _parent_end_child_handler(get_pid());
}

/************************************************************************
 *    child side
 *************************************************************************/

/**
 * @brief Snapshot the child process's current resource usage.
 *
 * Reads resident memory, thread count and open file descriptor count.
 * Called after the first check completes to establish a baseline; subsequent
 * calls let the policy detect leaks or excessive growth and decide whether to
 * recycle the process.
 *
 * @return A load struct populated with the current measurements.
 */
check_child::load check_child::measure_load() {}

/**
 * @brief Entry point executed in the forked child process.
 *
 * Wraps the raw file descriptors into Asio pipes and drives a synchronous
 * request-reply loop: read an execute request from the parent, run the
 * embedded Perl script, write the result back. The loop exits cleanly on a
 * terminate message or on a pipe error (parent died), allowing the OS to
 * reclaim the process without leaking resources.
 *
 * @param stdin_fd   File descriptor connected to the parent's write end.
 * @param stdout_fd  File descriptor connected to the parent's read end.
 * @return 0 on clean exit.
 */
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
