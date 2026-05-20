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

#include <re2/re2.h>

#include "com/centreon/connector/perl/check_child.hh"

#include <EXTERN.h>
#include <perl.h>
#include <spdlog/spdlog.h>
#include <unistd.h>

#include "common/inc/com/centreon/common/process_stat.hh"
#include "src/perl_connector.pb.h"

extern PerlInterpreter* my_perl;

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
void check_child::_on_stdout_read(const boost::system::error_code& err,
                                  const std::string received) {
  if (err) {
    kill();
    return;
  }
  std::vector<ConnectorMess> decoded;

  auto forward_to_handler = [&, this]() {
    for (const ConnectorMess& to_read : decoded) {
      SPDLOG_LOGGER_DEBUG(_logger, "{} receive from check_child pid={}: {}",
                          _script_path, get_pid(), to_read.ShortDebugString());
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

void check_child::execute(const ConnectorMess& stdin_mess) {
  _running = true;
  ++_execute_counter;
  auto raw = _protocol.serialize(stdin_mess);
  write_to_child_stdin(
      std::string_view(reinterpret_cast<const char*>(raw.get()), raw->len));
}

/**
 * @brief Called by the parent when the child process exits (normal or
 * abnormal).
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
check_child::load check_child::measure_load() {
  try {
    common::process_stat stats(getpid());
    return load{.used_memory = stats.res_size(),
                .nb_thread = stats.num_threads(),
                .nb_opened_fd = stats.opened_fds()};
  } catch (const std::exception& e) {
    std::cout << "fail process stats:" << e.what() << std::endl;
    throw;
  }
}

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
  SPDLOG_LOGGER_DEBUG(_logger, "pid: {} check_child start", get_pid());
  asio::readable_pipe child_stdin(*_io_context, stdin_fd);
  asio::writable_pipe child_stdout(*_io_context, stdout_fd);

  // we redirect stdout output of perl script to a pipe
  int stdout_pipe_fd[2];
  if (pipe(stdout_pipe_fd)) {
    return -1;
  }
  fcntl(stdout_pipe_fd[0], F_SETPIPE_SZ, 0x100000);  // 1 MiB
  dup2(stdout_pipe_fd[1], STDOUT_FILENO);
  close(stdout_pipe_fd[1]);
  int stderr_pipe_fd[2];
  if (pipe(stderr_pipe_fd)) {
    return -1;
  }
  dup2(stderr_pipe_fd[1], STDERR_FILENO);
  close(stderr_pipe_fd[1]);

  dSP;
  SPAGAIN;

  while (1) {
    ConnectorMess received;
    boost::system::error_code err = _protocol.recv(child_stdin, received);
    if (err) {
      break;
    }
    SPDLOG_LOGGER_TRACE(_logger, "pid: {} check_child receive: {}", getpid(),
                        received.ShortDebugString());
    if (received.has_terminate()) {
      break;
    }
    if (received.has_execute()) {
      ENTER;
      SAVETMPS;
      PUSHMARK(SP);
      XPUSHs(reinterpret_cast<SV*>(_check_script_handle));
      XPUSHs(sv_2mortal(newSVpv(_script_path.c_str(), 0)));
      for (const auto& arg : received.execute().args()) {
        XPUSHs(sv_2mortal(newSVpv(arg.c_str(), 0)));
      }
      PUTBACK;
      call_pv("Embed::Persistent::run_file", G_DISCARD);
      SPAGAIN; /* rafraichit le pointeur de pile  */
      LEAVE;

      struct pollfd pfd[2];
      pfd[0].fd = stdout_pipe_fd[0];
      pfd[0].events = POLLIN;
      pfd[1].fd = stderr_pipe_fd[0];
      pfd[1].events = POLLIN;

      ConnectorMess result;
      auto res = result.mutable_result();
      res->set_cmd_id(received.execute().cmd_id());
      res->set_pid(getpid());

      int poll_ret;
      char buffer[4096];
      size_t nb_read;
      bool stdout_received = false;
      bool stderr_received = false;
      bool status_decoded = false;
      // we wait 1000ms to receive first stdout and stderr datas
      while ((poll_ret = poll(
                  pfd, 2, (stdout_received && stderr_received) ? 10 : 1000))) {
        if (pfd[0].revents & POLLIN) {
          nb_read = ::read(stdout_pipe_fd[0], buffer, sizeof(buffer));
          res->mutable_stdout()->append(buffer, nb_read);
          stdout_received = true;
        }
        if (pfd[1].revents & POLLIN) {
          nb_read = ::read(stderr_pipe_fd[0], buffer, sizeof(buffer));
          static re2::RE2 exit_code_pattern("SCRIPT_EXIT_CODE:(\\d+)");
          int exit_status = -1;
          if (re2::RE2::PartialMatch(std::string_view(buffer, nb_read),
                                     exit_code_pattern, &exit_status)) {
            res->set_status(exit_status);
            status_decoded = true;
          }
          stderr_received = true;
        }
      }
      if (!status_decoded) {
        SPDLOG_LOGGER_ERROR(_logger, "pid: {} fail to decode status", getpid());
        res->set_status(3);  // UNKNOWN
        res->set_stdout("script status no decoded " + res->stdout());
      }
      load new_load = measure_load();
      if (!_after_first_check_load) {
        _after_first_check_load = new_load;
      }
      res->mutable_afterfirstcheck()->set_nb_thread(
          _after_first_check_load->nb_thread);
      res->mutable_afterfirstcheck()->set_nb_opened_fd(
          _after_first_check_load->nb_opened_fd);
      res->mutable_afterfirstcheck()->set_used_memory(
          _after_first_check_load->used_memory);
      res->mutable_afterlastcheck()->set_nb_thread(new_load.nb_thread);
      res->mutable_afterlastcheck()->set_nb_opened_fd(new_load.nb_opened_fd);
      res->mutable_afterlastcheck()->set_used_memory(new_load.used_memory);
      SPDLOG_LOGGER_TRACE(_logger,
                          "pid: {} check_child send to script_child: {}",
                          getpid(), result.ShortDebugString());
      boost::system::error_code send_error =
          _protocol.send(child_stdout, result);
      if (send_error) {
        break;
      }
    }
  }
  SPDLOG_LOGGER_DEBUG(_logger, "end of check_child pid:{}", getpid());
  return 0;
}
