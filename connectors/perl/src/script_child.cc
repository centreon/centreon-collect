/**
 * Copyright 2026 Centreon
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

#include <sys/prctl.h>

#include "com/centreon/connector/perl/check_child.hh"
#include "com/centreon/connector/perl/script_child.hh"
#include "com/centreon/exceptions/msg_fmt.hh"
#include "common/inc/com/centreon/common/file_system.hh"
#include "src/perl_connector.pb.h"

#include <EXTERN.h>
#include <perl.h>
#include <cstddef>
#include <cstring>
#include <ctime>
#include <memory>

using namespace com::centreon;
using namespace com::centreon::connector::perl;

// Perl interpreter object.
PerlInterpreter* my_perl(nullptr);
// Allow module loading.
EXTERN_C void xs_init(pTHX);

constexpr std::string_view _LOADER_SCRIPT = R"(
#!/usr/bin/perl

package Embed::Persistent;

use Text::ParseWords qw(parse_line);

use constant MTIME_IDX  => 0;
use constant HANDLE_IDX => 1;

$| = 1;

sub valid_package_name {
  my ($string) = @_;
  # First pass.
  $string =~ s/([^A-Za-z0-9\/])/sprintf("_%2x", unpack("C", $1))/eg;
  # Second pass only for words starting with a digit.
  $string =~ s|/(\d)|sprintf("/_%2x", unpack("C", $1))|eg;
  # Dress it up as a real package name.
  $string =~ s|/|::|g;
  return "Embed" . $string;
}

sub eval_file {
  my ($filename) = @_;
  my $mtime = -M $filename;

  # Read Perl script.
  my $package = valid_package_name($filename);
  open(my $fh, "<", $filename)
    or die "failed to open Perl file '$filename': $!";
  my $sub;
  sysread $fh, $sub, -s $fh;
  close $fh;
  $sub =~ s/__END__/\;}
__END__/;

  # Wrap the code into a subroutine.
  my $hndlr = <<EOSUB;
package $package;


BEGIN {
    *CORE::GLOBAL::exit = sub {
        print STDERR "SCRIPT_EXIT_CODE:", \$_[0], "\\n";
        die "EXIT \$_[0]\n";
    };
}

sub subroutine {
  \@ARGV = \@_;
  local \$^W = 1;
  $sub
}
EOSUB

 # Ensure modified Perl plugins get recached properly.
  no strict 'refs';
  undef %{$package.'::'};
  use strict 'refs';

  # Compile.
  eval $hndlr;
  if ($@) {
    chomp($@);
    die "syntax error in '$filename': $@";
  }

  no strict 'refs';
  return *{ $package . '::subroutine' }{CODE} ;
}

sub run_file {
  # Fetch arguments.
  my ($handle, @parsed_args) = @_ ;
  
  # Run subroutine.
  # print "Run subroutine";
  my $res;
  eval { $res = $handle->(@parsed_args) };
  # if ($@) {
  #   chomp($@);
  #   die "could not run '$filename': $@";
  # }
  return ($res);
}

)";

/************************************************************************
 *    parent side
 *************************************************************************/

constexpr std::string_view _SCRIPT_PATH = "/tmp/centreon_connector_perl.XXXXXX";

/**
 * @brief Destroy the script_child object and shut down the Perl interpreter.
 *
 * Forces a full Perl destruction level so global cleanup hooks (END blocks,
 * destructors) are run before freeing the interpreter memory.
 */
script_child::~script_child() {
  SPDLOG_LOGGER_INFO(_logger, "cleaning up Embedded Perl");
  if (my_perl) {
    PL_perl_destruct_level = 1;
    perl_destruct(my_perl);
    perl_free(my_perl);
    PERL_SYS_TERM();
  }
  if (!_loader_script_path.empty()) {
    unlink(_loader_script_path.c_str());
  }
}

/**
 * @brief Write the embedded loader script to a temporary file on disk.
 *
 * Combines the built-in _LOADER_SCRIPT with any @p additional_code and writes
 * the result to a unique temp file created via mkstemp. The file is used as
 * the argv[1] argument when parsing the Perl interpreter in _compile_script().
 *
 * @param additional_code  Optional Perl code appended after the loader body.
 * @return Absolute path to the temporary file.
 * @throws com::centreon::exceptions::msg_fmt if the file cannot be created or
 *         written.
 */
std::string script_child::_write_loader_to_disk(
    const std::string_view& additional_code) {
  char script_path[_SCRIPT_PATH.length() + 1];
  strcpy(script_path, _SCRIPT_PATH.data());
  int script_fd = mkstemp(script_path);
  if (script_fd < 0) {
    char const* msg(strerror(errno));
    throw exceptions::msg_fmt(
        "could not create temporary file for loader {}: {}", script_path, msg);
  }

  std::list<std::string_view> contents;
  contents.push_back(_LOADER_SCRIPT);
  if (!additional_code.empty()) {
    contents.push_back(additional_code);
    contents.push_back("\n");
  }

  for (const std::string_view content : contents) {
    size_t len = content.length();
    size_t offset = 0;
    while (len > 0) {
      ssize_t wb(write(script_fd, content.data() + offset, len));
      if (wb <= 0) {
        char const* msg(strerror(errno));
        close(script_fd);
        unlink(script_path);
        throw exceptions::msg_fmt("could not write embedded script {}: {}",
                                  script_path, msg);
      }
      len -= wb;
      offset += wb;
    }
  }
  close(script_fd);
  return script_path;
}

/**
 * @brief Allocate and initialise the embedded Perl interpreter.
 *
 * Parses the loader script written by _write_loader_to_disk() so that the
 * Embed::Persistent package is available for subsequent eval_file calls.
 * PL_origalen is set to 1 to prevent Perl from reusing argv memory, and
 * PERL_EXIT_DESTRUCT_END ensures END blocks run at interpreter destruction.
 *
 * @param loader_path  Path to the temporary loader script on disk.
 * @throws com::centreon::exceptions::msg_fmt if the interpreter cannot be
 *         allocated or the script cannot be parsed.
 */
void script_child::_compile_script(const std::string& loader_path) {
  SPDLOG_LOGGER_INFO(_logger, "loading Embedded Perl interpreter");

  if (!(my_perl = perl_alloc())) {
    SPDLOG_LOGGER_ERROR(_logger, "could not allocate Perl interpreter");
  }
  perl_construct(my_perl);
  PL_origalen = 1;
  PL_perl_destruct_level = 1;

  // Parse embedded script.
  const char* embedding[2];
  embedding[0] = "";
  embedding[1] = loader_path.c_str();
  if (perl_parse(my_perl, &xs_init, sizeof(embedding) / sizeof(*embedding),
                 (char**)embedding, nullptr)) {
    SPDLOG_LOGGER_ERROR(_logger, "could not parse embedded Perl script");
    throw exceptions::msg_fmt("could not parse embedded Perl script");
  }
  PL_exit_flags |= PERL_EXIT_DESTRUCT_END;
  perl_run(my_perl);
}

/**
 * @brief Compile the user-provided check script via the embedded Perl
 * interpreter.
 *
 * Reads the script file, calls Embed::Persistent::eval_file to compile it into
 * an anonymous subroutine, and stores the resulting code reference in
 * _check_script_handle. Also records the file mtime so
 * _every_second_timer_handler can detect later modifications and trigger a
 * reload.
 *
 * @throws com::centreon::exceptions::msg_fmt if the file cannot be read,
 *         eval_file returns no handle, or the Perl ERRSV is set after
 *         compilation.
 */
void script_child::_load_check_script() {
  // load check script
  std::filesystem::file_time_type check_script_mtime;
  try {
    check_script_mtime = std::filesystem::last_write_time(_script_path);
  } catch (const std::exception& e) {
    SPDLOG_LOGGER_ERROR(_logger, "failed to open Perl file {}", _script_path);
    throw exceptions::msg_fmt("failed to open Perl file {}", _script_path);
  }

  // Compile Perl file.
  dSP;
  {
    SPDLOG_LOGGER_DEBUG(_logger, "parsing file {}", _script_path);
    const char* argv[3];
    argv[0] = _script_path.c_str();
    argv[1] = "0";
    argv[2] = nullptr;
    if (call_argv("Embed::Persistent::eval_file", G_EVAL | G_SCALAR,
                  (char**)argv) != 1)
      throw exceptions::msg_fmt("could not compile Perl script {}",
                                _script_path);
  }
  SPAGAIN;
  SV* handle = POPs;
  // we had a reference to handle in order that perl GC will not erase it
  SvREFCNT_inc(handle);
  if (SvTRUE(ERRSV)) {
    throw exceptions::msg_fmt("Embedded Perl error: {}", SvPV_nolen(ERRSV));
  }
  _check_script_mtime = check_script_mtime;
  _check_script_handle = handle;
}

/**
 * @brief Serialize a protobuf message and write it synchronously to the child's
 * stdin.
 *
 * Used by the parent process to send execute or terminate requests to this
 * script_child instance.
 *
 * @param to_send  Message to serialize and forward.
 */
void script_child::write_mess_to_child_stdin(const ConnectorMess& to_send) {
  auto buff = _protocol.serialize(to_send);
  write_to_child_stdin(
      std::string_view(reinterpret_cast<const char*>(buff.get()), buff->len));
}

/**
 * @brief Called by the parent when data arrives on this child's stdout pipe.
 *
 * Decodes one or more protobuf messages and forwards each one to the
 * registered parent handler. On decode failure the child is killed first so
 * the parent is not left waiting for a result that will never come.
 *
 * @param raw_data  Raw bytes read from the child stdout pipe.
 */
void script_child::_on_stdout_read(const boost::system::error_code& err,
                                   const std::string raw_data) {
  if (err) {  // child is dying, wait for process end
    return;
  }
  std::vector<ConnectorMess> received;

  auto forward_to_handler = [&, this]() {
    for (const ConnectorMess& to_read : received) {
      SPDLOG_LOGGER_DEBUG(_logger, "{} receive from script_child: {}",
                          _script_path, to_read.ShortDebugString());
      _parent_read_handler(shared_from_this(), to_read);
    }
  };

  try {
    _protocol.on_recv(raw_data, received);
    forward_to_handler();
  } catch (const std::exception& e) {  // something wrong => kill child
    SPDLOG_LOGGER_ERROR(
        _logger, "exception on decode data from script child: {}", e.what());
    kill();
    forward_to_handler();
  }
}

/**
 * @brief Called by the parent when this child process exits.
 *
 * Notifies the owning policy so it can remove this instance from its
 * active-children map and stop dispatching requests to it.
 */
void script_child::_on_process_end() {
  _parent_end_child_handler(shared_from_this());
}

/************************************************************************
 *    child side
 *************************************************************************/

/**
 * @brief Entry point executed in the forked child process.
 *
 * Initialises the Perl interpreter in three steps: write the loader to disk,
 * compile it, then compile the check script. Any failure is reported back to
 * the parent via a have_to_terminate message before the process exits with -1.
 * On success, the stdin pipe is opened, the minute timer is armed, the process
 * name is set to the script basename, and the asio event loop is started.
 *
 * @param stdin_fd   File descriptor for commands received from the parent.
 * @param stdout_fd  File descriptor for results sent back to the parent.
 * @return 0 on clean exit, -1 if initialisation failed.
 */
int script_child::_run(int stdin_fd, int stdout_fd, int) {
  SPDLOG_LOGGER_DEBUG(_logger, "start of script_child {} pid={}", _script_path,
                      getpid());
  if (_fd_to_close_after_fork >= 0) {
    ::close(_fd_to_close_after_fork);
  }
  _child_io_context = std::make_shared<asio::io_context>();
  _child_stdout =
      std::make_unique<asio::writable_pipe>(*_child_io_context, stdout_fd);
  try {
    _loader_script_path = _write_loader_to_disk(_config.code());
  } catch (const std::exception& e) {
    _global_error = e.what();
  }
  if (_global_error.empty()) {
    try {
      _compile_script(_loader_script_path);
    } catch (const std::exception& e) {
      _global_error = e.what();
    }
  }
  if (_global_error.empty()) {
    try {
      _load_check_script();
    } catch (const std::exception& e) {
      _global_error = e.what();
    }
  }
  if (!_global_error.empty()) {
    ConnectorMess error;
    error.mutable_have_to_terminate()->set_error(_global_error);
    auto dummy [[maybe_unused]] = _protocol.send(*_child_stdout, error);
    std::this_thread::sleep_for(std::chrono::seconds(1));
    return -1;
  }
  _child_stdin =
      std::make_unique<asio::readable_pipe>(*_child_io_context, stdin_fd);
  read_from_main_process_stdin();
  _every_second_timer =
      std::make_unique<asio::system_timer>(*_child_io_context);
  _start_every_second_timer();

  // set process name
  auto basename = std::filesystem::path(_script_path).filename();
  // top
  prctl(PR_SET_NAME, basename.c_str());
  // ps
  if (_argv0) {
    size_t max_len = strlen(_argv0);
    strncpy(_argv0, basename.c_str(), max_len);
  }

  _child_io_context->run();

  for (const check_child_last_used& child : _check_childs) {
    child.child->kill();
  }

  for (const auto& child : _die_start_to_check_child) {
    child.second.to_kill->kill();
  }

  SPDLOG_LOGGER_DEBUG(_logger, "end of script_child {} pid={}", _script_path,
                      getpid());
  return 0;
}

/**
 * @brief Post an async read on the child stdin pipe.
 *
 * Each call consumes exactly one incoming message; the completion handler
 * (_on_stdin_receive) re-posts another read to keep the loop alive.
 */
void script_child::read_from_main_process_stdin() {
  _protocol.async_recv(*_child_stdin,
                       [me = shared_from_this()](
                           const boost::system::error_code& err,
                           const std::shared_ptr<ConnectorMess>& received) {
                         me->_on_stdin_receive(err, received);
                         if (!err) {
                           me->read_from_main_process_stdin();
                         }
                       });
}

/**
 * @brief Arm the one-minute periodic timer.
 *
 * Called once after initialisation and re-armed at the end of each
 * _every_second_timer_handler invocation to provide a recurring check.
 */
void script_child::_start_every_second_timer() {
  _every_second_timer->expires_after(std::chrono::seconds(1));
  _every_second_timer->async_wait(
      [this, me = shared_from_this()](const boost::system::error_code& err) {
        if (!err) {
          _every_second_timer_handler();
        }
      });
}

/**
 * @brief Fired every second to detect check script modifications.
 *
 * Compares the current mtime of the script file against the mtime recorded
 * at load time. If the file has changed, sends a have_to_terminate message to
 * the parent so it can restart this child with the updated script.
 * The timer is re-armed unconditionally at the end.
 */
void script_child::_every_second_timer_handler() {
  std::error_code err;
  auto check_script_mtime = std::filesystem::last_write_time(_script_path, err);
  if (!err &&
      check_script_mtime != _check_script_mtime) {  // script updated => reload
    SPDLOG_LOGGER_INFO(
        _logger,
        "script {} updated => live until all pending queries completed",
        _script_path);
    ConnectorMess error;
    error.mutable_have_to_terminate()->set_error(
        fmt::format("{} updated, need to reload", _script_path));
    _send_to_main_process(error);
  }

  // check execute timeout
  auto& timeout_index = _pending.get<1>();
  if (!timeout_index.empty()) {
    auto upper_limit = timeout_index.lower_bound(time(nullptr));
    for (auto timeout_iter = timeout_index.begin(); timeout_iter != upper_limit;
         ++timeout_iter) {
      SPDLOG_LOGGER_ERROR(_logger, "{} timeout on child pid={} => kill",
                          _script_path, timeout_iter->pid);
      auto& pid_index = _check_childs.get<0>();
      auto to_kill = pid_index.find(timeout_iter->pid);
      if (to_kill != pid_index.end()) {
        _kill_check_child(true, false, to_kill->child);
      }
      // send time out result to main process
      // we does not set pid because it will be killed
      ConnectorMess timeout_result;
      auto res = timeout_result.mutable_result();
      res->set_cmd_id(timeout_iter->query->execute().cmd_id());
      res->set_status(3);
      res->set_stderr("(Process Timeout)");
      _send_to_main_process(timeout_result);
    }
    timeout_index.erase(timeout_index.begin(), upper_limit);
    _clean_execute_queue();
  }

  // kill time out
  if (!_die_start_to_check_child.empty()) {
    std::vector<std::shared_ptr<check_child>> to_final_kill;
    auto limit = _die_start_to_check_child.lower_bound(time(nullptr) - 10);
    for (auto to_kill = _die_start_to_check_child.begin(); to_kill != limit;
         ++to_kill) {
      if (to_kill->second.final) {  // sigterm yet sent?
        SPDLOG_LOGGER_ERROR(_logger,
                            "script: {} kill (SIGKILL) check child pid={}",
                            _script_path, to_kill->second.to_kill->get_pid());
        to_kill->second.to_kill->kill();
      } else {
        to_final_kill.emplace_back(to_kill->second.to_kill);
      }
    }
    _die_start_to_check_child.erase(_die_start_to_check_child.begin(), limit);
    for (std::shared_ptr<check_child> to_insert : to_final_kill) {
      _die_start_to_check_child.emplace(time(nullptr),
                                        killing_check_child(true, to_insert));
    }
  }

  // after x minutes of inactivity => kill check child
  if (!_check_childs.empty()) {
    auto& last_used_index = _check_childs.get<1>();
    auto too_idle = last_used_index.lower_bound(
        time(nullptr) - 60 * _config.minute_idle_check_child_ttl());
    for (auto to_kill = last_used_index.begin(); to_kill != too_idle;
         ++to_kill) {
      _kill_check_child(false, true, to_kill->child);
    }
    last_used_index.erase(last_used_index.begin(), too_idle);
  }

  _start_every_second_timer();
}

void script_child::_create_child_and_execute(
    const std::shared_ptr<ConnectorMess>& query) {
  std::shared_ptr<check_child> new_child = std::make_shared<check_child>(
      _child_io_context, _logger, _script_path, _check_script_handle,
      [weak_me = weak_from_this()](int pid,
                                   const ConnectorMess& from_check_child) {
        auto me = weak_me.lock();
        if (me) {
          std::static_pointer_cast<script_child>(me)
              ->_from_child_script_receive(pid, from_check_child);
        }
      },
      [weak_me = weak_from_this()](int pid) {
        auto me = weak_me.lock();
        if (me) {
          std::static_pointer_cast<script_child>(me)->_on_child_script_end(pid);
        }
      });
  new_child->do_fork(false);
  _check_childs.emplace(new_child);
  SPDLOG_LOGGER_TRACE(_logger, "{} new child_script pid={} will do the job",
                      _script_path, new_child->get_pid());
  new_child->execute(*query);
  _pending.emplace(new_child->get_pid(), query);
}

/**
 * @brief Dispatch a message received from the main process.
 *
 * For execute requests: forwards to the first idle check_child. If none is
 * idle and the request forbids creating a new child, the request is queued
 * ordered by timeout; otherwise a new check_child is forked. For
 * have_to_terminate requests: kills the target check_child immediately or
 * sends it a graceful terminate message. Always re-posts a read at the end
 * to keep the stdin loop alive.
 *
 * @param err                    Asio error code; non-zero means the pipe is
 *                               broken and the io_context is stopped.
 * @param from_main_process_mess Message received from the main process.
 */
void script_child::_on_stdin_receive(
    const boost::system::error_code& err,
    const std::shared_ptr<ConnectorMess>& from_main_process_mess) {
  if (err) {
    SPDLOG_LOGGER_ERROR(_logger,
                        "script_child {} fail receive from main process {}",
                        _script_path, err.message());
    _child_io_context->stop();
    return;
  }
  SPDLOG_LOGGER_DEBUG(_logger, "script_child {} receive {}", _script_path,
                      from_main_process_mess->ShortDebugString());
  if (from_main_process_mess->has_execute()) {  // EXECUTE
    // we search idle check_child that have less number of perl execute
    auto& nb_execute_index = _check_childs.get<2>();
    for (const auto& idle : nb_execute_index) {
      if (!idle.child->is_running()) {
        SPDLOG_LOGGER_TRACE(_logger,
                            "{} idle child_script pid={} will do the job",
                            _script_path, idle.get_pid());
        idle.child->execute(*from_main_process_mess);
        _pending.emplace(idle.get_pid(), from_main_process_mess);
        return;
      }
    }
    // no idle child
    if (from_main_process_mess->execute().no_child_create()) {
      _execute_queue.emplace(from_main_process_mess);
      SPDLOG_LOGGER_TRACE(_logger, "{} no idle child_script => enqueue",
                          _script_path);

    } else {
      _create_child_and_execute(from_main_process_mess);
    }
  } else if (from_main_process_mess->has_terminate()) {  // TERMINATE
    const auto& term = from_main_process_mess->terminate();
    if (getpid() == term.pid()) {  // terminate script child
      _child_io_context->stop();
    } else {
      auto& pid_index = _check_childs.get<0>();
      if (term.pid()) {  // terminate a specific check child
        auto dest = pid_index.find(term.pid());
        if (dest != pid_index.end()) {
          if (term.immediate()) {
            dest->child->kill();
          } else {
            _kill_check_child(true, true, dest->child);
          }
        }
      } else if (!term.other_pids()
                      .empty()) {  // terminate the first check child we can
        for (int64_t child_pid : term.other_pids()) {
          auto dest = pid_index.find(child_pid);
          if (dest != pid_index.end()) {
            if (!dest->child->is_running()) {
              _kill_check_child(true, !term.immediate(), dest->child);
              return;
            }
          }
        }
        // no idle found first pid will be killed asap
        for (int64_t child_pid : term.other_pids()) {
          auto dest = pid_index.find(child_pid);
          if (dest != pid_index.end()) {
            _kill_check_child(true, true, dest->child);
            return;
          }
        }
      }
    }
  }
}

/**
 * @brief Handle a message received from a check_child worker.
 *
 * On result: removes the pid from the pending map, forwards the result to
 * the main process, prunes timed-out queued requests, and immediately
 * dispatches the oldest queued request to the now-idle child if one exists.
 *
 * @param pid               PID of the check_child that sent the message.
 * @param from_child_script Message sent by the check_child.
 */
void script_child::_from_child_script_receive(
    int pid,
    const ConnectorMess& from_child_script) {
  SPDLOG_LOGGER_DEBUG(_logger, "{} receive from child_script pid={}: {}",
                      _script_path, pid, from_child_script.ShortDebugString());
  if (from_child_script.has_result()) {
    const auto& res = from_child_script.result();
    auto query = _pending.find(pid);
    auto& pid_index = _check_childs.get<0>();
    auto check_chld = pid_index.find(pid);
    if (check_chld != pid_index.end()) {
      // we keep iterator has pid is not modified
      pid_index.modify(check_chld, [](check_child_last_used& to_update) {
        to_update.execute_counter = to_update.child->execute_counter();
        to_update.last_used = time(nullptr);
      });
    }
    enum {
      no_kill,
      max_execute,
      max_memory,
      max_fd,
      max_thread
    } kill_reason = no_kill;
    if (query != _pending.end() && check_chld != pid_index.end()) {
      const auto& execute = query->query->execute();
      if (execute.max_execute()) {
        if (execute.max_execute() <= check_chld->child->execute_counter()) {
          SPDLOG_LOGGER_DEBUG(_logger, "{}: too much executions pid:{} => kill",
                              _script_path, pid);
          kill_reason = max_execute;
        }
      }
      if (kill_reason == no_kill && execute.percent_max_memory_increased()) {
        if (res.after_first_check().used_memory() *
                (1.0 + execute.percent_max_memory_increased() / 100.0) <
            res.after_last_check().used_memory()) {
          SPDLOG_LOGGER_DEBUG(_logger,
                              "{}: too much memory growth pid:{} => kill",
                              _script_path, pid);
          kill_reason = max_memory;
        }
      }
      if (kill_reason == no_kill && execute.percent_max_open_fd_increased()) {
        if (res.after_first_check().nb_opened_fd() *
                (1.0 + execute.percent_max_open_fd_increased() / 100.0) <
            res.after_last_check().nb_opened_fd()) {
          SPDLOG_LOGGER_DEBUG(_logger,
                              "{}: too much opened fd growth pid:{} => kill",
                              _script_path, pid);
          kill_reason = max_fd;
        }
      }
      if (kill_reason == no_kill && execute.max_thread()) {
        if (res.after_last_check().nb_thread() >= execute.max_thread()) {
          SPDLOG_LOGGER_DEBUG(_logger, "{}: too much threads pid:{} => kill",
                              _script_path, pid);
          kill_reason = max_thread;
        }
      }
      _pending.erase(query);
      if (kill_reason != no_kill) {
        _kill_check_child(true, true, check_chld->child);
      }
    }
    _send_to_main_process(from_child_script);

    _clean_execute_queue();
    if (!_execute_queue.empty()) {
      if (check_chld != pid_index.end() && kill_reason == no_kill) {
        auto to_send = _execute_queue.extract(_execute_queue.begin());
        check_chld->child->execute(*to_send.value());
        _pending.emplace(pid, to_send.value());
      }
    }
  }
}

/**
 * @brief Initiate the termination of a check_child process.
 *
 * Optionally removes the child from the active map, then moves it to
 * _die_start_to_check_child (timestamped so a watchdog can detect stalls)
 * and sends it a child_end message so it exits cleanly.
 *
 * @param erase_from_check_childs  If true, remove the entry from
 * _check_childs before killing. Pass false to not create another one on child
 * end.
 * @param send_child_end_mess  If true, sends a child_end message
 * @param to_kill                  The check_child instance to terminate.
 */
void script_child::_kill_check_child(bool erase_from_check_childs,
                                     bool send_child_end_mess,
                                     std::shared_ptr<check_child> to_kill) {
  if (erase_from_check_childs) {
    _check_childs.get<0>().erase(to_kill->get_pid());
  }
  if (send_child_end_mess) {
    ConnectorMess kill_mess;
    kill_mess.mutable_terminate()->set_pid(to_kill->get_pid());
    to_kill->execute(kill_mess);
    _die_start_to_check_child.emplace(time(nullptr),
                                      killing_check_child(false, to_kill));
  } else {
    SPDLOG_LOGGER_INFO(_logger, "script: {} kill (SIGTERM) check child pid={}",
                       _script_path, to_kill->get_pid());
    to_kill->request_exit();
    _die_start_to_check_child.emplace(time(nullptr),
                                      killing_check_child(true, to_kill));
  }
}

/**
 * @brief Discard timed-out entries from the pending execute queue.
 *
 * Iterates from the front of _execute_queue (ordered by timeout) and drops
 * every check whose deadline has already passed. Called before scheduling new
 * work so stale checks are never dispatched to a fresh child process.
 */
void script_child::_clean_execute_queue() {
  if (!_execute_queue.empty()) {
    auto limit = _execute_queue.lower_bound(time(nullptr));
    _execute_queue.erase(_execute_queue.begin(), limit);
  }
}

/**
 * @brief Called when a check_child process exits.
 *
 * Removes the child from the active map. If a check was still pending for
 * this pid (i.e. the child died before sending a result), synthesizes an
 * unknown-status result and forwards it to the main process so the check
 * does not hang indefinitely.
 *
 * @param pid  PID of the check_child that exited.
 */
void script_child::_on_child_script_end(int pid) {
  for (auto child_to_kill = _die_start_to_check_child.begin();
       child_to_kill != _die_start_to_check_child.end(); ++child_to_kill) {
    if (child_to_kill->second.to_kill->get_pid() == pid) {
      _die_start_to_check_child.erase(child_to_kill);
      break;
    }
  }

  auto& child_pid_index = _check_childs.get<0>();
  auto check_to_delete = child_pid_index.find(pid);
  const bool was_registered = (check_to_delete != child_pid_index.end());
  if (was_registered) {
    child_pid_index.erase(check_to_delete);
  }
  auto& pid_index = _pending.get<0>();
  auto pending = pid_index.find(
      pid);  // process end while running check => generate status
  if (pending != pid_index.end()) {
    SPDLOG_LOGGER_ERROR(_logger, "{} end of check_child during check pid={}",
                        _script_path, pid);
    ConnectorMess bad_terminate;
    auto res = bad_terminate.mutable_result();
    res->set_cmd_id(pending->query->execute().cmd_id());
    res->set_status(3);
    res->set_stdout(
        fmt::format("Process pid:{} died during check execution", pid));
    _send_to_main_process(bad_terminate);
    pid_index.erase(pending);
  } else {
    SPDLOG_LOGGER_DEBUG(_logger, "{} end of check_child pid={}", _script_path,
                        pid);
  }
  // replace dead check_child by another if queries in queue
  if (was_registered && !_execute_queue.empty()) {
    auto to_send = _execute_queue.extract(_execute_queue.begin());
    _create_child_and_execute(to_send.value());
  }

  // then report to main process
  ConnectorMess to_main_process;
  to_main_process.mutable_child_end()->set_pid(pid);
  _send_to_main_process(to_main_process);
}

/**
 * @brief Asynchronously send a protobuf message to the main process via
 * stdout.
 *
 * Stops the io_context on write error so the child exits cleanly rather than
 * looping on a broken pipe.
 *
 * @param to_send  Message to serialize and send.
 */
void script_child::_send_to_main_process(const ConnectorMess& to_send) {
  SPDLOG_LOGGER_DEBUG(_logger, "{} send {} to main process", _script_path,
                      to_send.ShortDebugString());
  _protocol.async_send(
      *_child_stdout, to_send,
      [me = shared_from_this()](const boost::system::error_code err) {
        if (err) {
          SPDLOG_LOGGER_ERROR(me->_logger, "{} fail to send to main process",
                              me->_script_path);
          me->_child_io_context->stop();
        }
      });
}
