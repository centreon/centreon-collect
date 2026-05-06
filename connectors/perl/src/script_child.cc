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
  print "Run subroutine";
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
 * _check_script_handle. Also records the file mtime so _minute_timer_handler
 * can detect later modifications and trigger a reload.
 *
 * @throws com::centreon::exceptions::msg_fmt if the file cannot be read,
 *         eval_file returns no handle, or the Perl ERRSV is set after
 *         compilation.
 */
void script_child::_load_check_script() {
  // load check script
  std::filesystem::file_time_type check_script_mtime;
  std::string file_content;
  try {
    check_script_mtime = std::filesystem::last_write_time(_script_path);
    file_content = common::read_file_content(_script_path);
  } catch (const std::exception& e) {
    SPDLOG_LOGGER_ERROR(_logger, "could not load check script {}",
                        _script_path);
    throw exceptions::msg_fmt("could not load check script {}", _script_path);
  }

  // Compile Perl file.
  dSP;
  {
    SPDLOG_LOGGER_DEBUG(_logger, "parsing file {}", _script_path);
    const char* argv[3];
    argv[0] = file_content.c_str();
    argv[1] = "0";
    argv[2] = nullptr;
    if (call_argv("Embed::Persistent::eval_file", G_EVAL | G_SCALAR,
                  (char**)argv) != 1)
      throw exceptions::msg_fmt("could not compile Perl script {}",
                                _script_path);
  }
  SPAGAIN;
  SV* handle = POPs;
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
      std::string(reinterpret_cast<const char*>(buff.get()), buff->len));
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
void script_child::_on_stdout_read(const std::string raw_data) {
  std::vector<ConnectorMess> received;

  auto forward_to_handler = [&, this]() {
    for (const ConnectorMess& to_read : received) {
      _parent_read_handler(_script_path, to_read);
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
  _parent_end_child_handler(_script_path);
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
  _child_stdout =
      std::make_unique<asio::writable_pipe>(*_io_context, stdout_fd);
  std::string loader_path;
  try {
    loader_path = _write_loader_to_disk(_additional_code);
  } catch (const std::exception& e) {
    _global_error = e.what();
  }
  if (_global_error.empty()) {
    try {
      _compile_script(loader_path);
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
  _child_stdin = std::make_unique<asio::readable_pipe>(*_io_context, stdin_fd);
  read_from_main_process_stdin();

  auto basename = std::filesystem::path(_script_path).filename();
  prctl(PR_SET_NAME, basename.c_str());

  _io_context->run();
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
                       });
}

/**
 * @brief Arm the one-minute periodic timer.
 *
 * Called once after initialisation and re-armed at the end of each
 * _minute_timer_handler invocation to provide a recurring check.
 */
void script_child::_start_minute_timer() {
  _minute_timer.expires_after(std::chrono::minutes(1));
  _minute_timer.async_wait(
      [this, me = shared_from_this()](const boost::system::error_code& err) {
        if (!err) {
          _minute_timer_handler();
        }
      });
}

/**
 * @brief Fired every minute to detect check script modifications.
 *
 * Compares the current mtime of the script file against the mtime recorded
 * at load time. If the file has changed, sends a have_to_terminate message to
 * the parent so it can restart this child with the updated script.
 * The timer is re-armed unconditionally at the end.
 */
void script_child::_minute_timer_handler() {
  std::error_code err;
  auto check_script_mtime = std::filesystem::last_write_time(_script_path, err);
  if (!err &&
      check_script_mtime != _check_script_mtime) {  // script updated => reload
    ConnectorMess error;
    error.mutable_have_to_terminate()->set_error(
        fmt::format("{} updated, need to reload", _script_path));
    _protocol.async_send(*_child_stdout, error,
                         [](const boost::system::error_code&) {});
  }

  _start_minute_timer();
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
    _io_context->stop();
    return;
  }
  if (from_main_process_mess->has_execute()) {
    for (auto idle : _check_childs) {
      if (!idle.second->is_running()) {
        idle.second->execute(
            *from_main_process_mess,
            [weak_me = weak_from_this(), pid = idle.first,
             from_main_process_mess](const boost::system::error_code& err) {
              if (err) {
                auto me = weak_me.lock();
                if (me) {
                  std::static_pointer_cast<script_child>(me)
                      ->_on_execute_send_error(pid, from_main_process_mess);
                }
              }
            });
        _pending[idle.first] = from_main_process_mess;
        read_from_main_process_stdin();
        return;
      }
    }
    // no idle child
    if (from_main_process_mess->execute().no_child_create()) {
      _execute_queue.emplace(from_main_process_mess);
    } else {
      std::shared_ptr<check_child> new_child = std::make_shared<check_child>(
          _io_context, _logger, _script_path, _check_script_handle,
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
              std::static_pointer_cast<script_child>(me)->_on_child_script_end(
                  pid);
            }
          });
      new_child->do_fork(false);
      _check_childs.emplace(new_child->get_pid(), new_child);
      new_child->execute(
          *from_main_process_mess,
          [weak_me = weak_from_this(), pid = new_child->get_pid(),
           from_main_process_mess](const boost::system::error_code& err) {
            if (err) {
              auto me = weak_me.lock();
              if (me) {
                std::static_pointer_cast<script_child>(me)
                    ->_on_execute_send_error(pid, from_main_process_mess);
              }
            }
          });
      _pending[new_child->get_pid()] = from_main_process_mess;
    }
  } else if (from_main_process_mess->has_have_to_terminate()) {
    auto dest = _check_childs.find(from_main_process_mess->terminate().pid());
    if (dest == _check_childs.end()) {
      if (from_main_process_mess->terminate().immediate()) {
        dest->second->kill();
      } else {
        dest->second->execute(
            *from_main_process_mess,
            [me = shared_from_this()](const boost::system::error_code&) {});
      }
    }
  }
  read_from_main_process_stdin();
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
  if (from_child_script.has_result()) {
    _pending.erase(pid);
    _send_to_main_process(from_child_script);
    // timeout pending request? (answer will be sent by main process)
    time_t now = time(nullptr);
    while (!_execute_queue.empty() &&
           (*_execute_queue.begin())->execute().timeout() < now) {
      _execute_queue.erase(_execute_queue.begin());
    }
    if (!_execute_queue.empty()) {
      auto check_child = _check_childs.find(pid);
      if (check_child != _check_childs.end()) {
        auto to_send = _execute_queue.extract(_execute_queue.begin());
        check_child->second->execute(
            from_child_script, [weak_me = weak_from_this(), pid,
                                from_main_process_mess = to_send.value()](
                                   const boost::system::error_code& err) {
              if (err) {
                auto me = weak_me.lock();
                if (me) {
                  std::static_pointer_cast<script_child>(me)
                      ->_on_execute_send_error(pid, from_main_process_mess);
                }
              }
            });
        _pending.emplace(pid, to_send.value()).first;
      }
    }
  }
}

/**
 * @brief Called when sending an execute request to a check_child fails.
 *
 * Kills the offending child and removes it from the active-children map.
 * The pending request is implicitly abandoned; _on_child_script_end will
 * generate an unknown-status result for it when the child exits.
 *
 * @param pid   PID of the check_child that could not receive the request.
 * @param mess  The execute request that failed to be sent (unused here).
 */
void script_child::_on_execute_send_error(
    int pid,
    const std::shared_ptr<ConnectorMess>& mess) {
  auto to_kill = _check_childs.find(pid);
  to_kill->second->kill();
  _check_childs.erase(to_kill);
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
  _check_childs.erase(pid);

  auto pending =
      _pending.find(pid);  // process end while running check => generate status
  if (pending != _pending.end()) {
    const Execute& command = pending->second->execute();
    ConnectorMess bad_terminate;
    auto res = bad_terminate.mutable_result();
    res->set_cmd_id(command.cmd_id());
    res->set_status(3);  // unknown
    res->set_stdout(
        fmt::format("{} had terminated without result", _script_path));

    _pending.erase(pending);
    _send_to_main_process(bad_terminate);
  }
}

/**
 * @brief Asynchronously send a protobuf message to the main process via stdout.
 *
 * Stops the io_context on write error so the child exits cleanly rather than
 * looping on a broken pipe.
 *
 * @param to_send  Message to serialize and send.
 */
void script_child::_send_to_main_process(const ConnectorMess& to_send) {
  _protocol.async_send(
      *_child_stdout, to_send,
      [me = shared_from_this()](const boost::system::error_code err) {
        if (err) {
          me->_io_context->stop();
        }
      });
}
