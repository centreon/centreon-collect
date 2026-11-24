/**
 * Copyright 1999-2009      Ethan Galstad
 * Copyright 2009-2012      Icinga Development Team (http://www.icinga.org)
 * Copyright 2011-2014,2016 Centreon
 *
 * This file is part of Centreon Engine.
 *
 * Centreon Engine is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation.
 *
 * Centreon Engine is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Centreon Engine. If not, see
 * <http://www.gnu.org/licenses/>.
 */

#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <csignal>
#include "absl/debugging/stacktrace.h"
#include "absl/debugging/symbolize.h"

#include "com/centreon/engine/broker.hh"
#include "com/centreon/engine/broker/loader.hh"
#include "com/centreon/engine/checks/checker.hh"
#include "com/centreon/engine/commands/connector.hh"
#include "com/centreon/engine/commands/raw_v2.hh"
#include "com/centreon/engine/comment.hh"
#include "com/centreon/engine/configuration/applier/state.hh"
#include "com/centreon/engine/enginerpc.hh"
#include "com/centreon/engine/events/loop.hh"
#include "com/centreon/engine/globals.hh"
#include "com/centreon/engine/shared.hh"
#include "com/centreon/engine/string.hh"
#include "common/downtimes/downtime_manager.hh"

using namespace com::centreon;
using namespace com::centreon::common::downtimes;
using namespace com::centreon::engine;
using namespace com::centreon::engine::events;

/******************************************************************/
/******************** SYSTEM COMMAND FUNCTIONS ********************/
/******************************************************************/

/* executes a system command - used for notifications, event handlers, etc. */
int my_system_r(macro_context& mac,
                std::string const& cmd,
                int timeout,
                bool* early_timeout,
                double* exectime,
                std::string& output,
                unsigned int max_output_length) {
  functions_logger->trace("my_system_r()");

  // initialize return variables.
  *early_timeout = false;
  *exectime = 0.0;

  // if no command was passed, return with no error.
  if (cmd.empty()) {
    return service::state_ok;
  }

  SPDLOG_LOGGER_DEBUG(commands_logger, "Running command '{}'...", cmd);

  timeval start_time = timeval();
  timeval end_time = timeval();

  // time to start command.
  gettimeofday(&start_time, nullptr);

  std::shared_ptr<commands::raw_v2> raw_cmd =
      std::make_shared<commands::raw_v2>(g_io_context, "system", cmd);
  commands::result res;
  raw_cmd->run(cmd, mac, timeout, res);

  end_time.tv_sec = res.end_time.to_seconds();
  end_time.tv_usec = res.end_time.to_useconds() - end_time.tv_sec * 1000000ull;
  *exectime = (res.end_time - res.start_time).to_seconds();
  *early_timeout = res.exit_status == common::e_exit_status::timeout;
  if (max_output_length > 0)
    output = res.output.substr(0, max_output_length - 1);
  else
    output = res.output;
  int result(res.exit_code);

  SPDLOG_LOGGER_DEBUG(
      commands_logger,
      "Execution time={:.3f} sec, early timeout={}, result={}, output={}",
      *exectime, *early_timeout, result, output);

  return result;
}

// same like unix ctime without the '\n' at the end of the string.
char const* my_ctime(time_t const* t) {
  char* buf(ctime(t));
  if (buf != nullptr)
    buf[strlen(buf) - 1] = 0;
  return buf;
}

/******************************************************************/
/******************** SIGNAL HANDLER FUNCTIONS ********************/
/******************************************************************/

/* trap signals so we can exit gracefully */
void setup_sighandler() {
  /* remove buffering from stderr, stdin, and stdout */
  setbuf(stdin, (char*)nullptr);
  setbuf(stdout, (char*)nullptr);
  setbuf(stderr, (char*)nullptr);

  /* initialize signal handling */
  signal(SIGPIPE, SIG_IGN);
  signal(SIGTERM, sighandler);
  signal(SIGHUP, sighandler);
}

/* handle signals */
void sighandler(int sig) {
  if (sig < 0)
    sig = -sig;

  int const sigs_size(sizeof(sigs) / sizeof(sigs[0]) - 1);
  sig_id = sig % sigs_size;

  /* we received a SIGHUP */
  if (sig_id == SIGHUP)
    sighup = true;
  /* else begin shutting down... */
  else
    sigshutdown = true;
}

/******************************************************************/
/************************ STRING FUNCTIONS ************************/
/******************************************************************/

/**
 *  @brief Determines whether or not an object name (host, service, etc)
 *  contains illegal characters.
 *
 *  This function uses the global illegal_object_chars variable. This is
 *  caused by the configuration reload mechanism which does not set the
 *  global configuration object itself until the end of the reload.
 *  However during the configuration reload, objects are still checked
 *  for invalid characters.
 *
 *  @param[in] name  Object name.
 *
 *  @return True if the object name contains an illegal character, false
 *          otherwise.
 */
bool contains_illegal_object_chars(const std::string& name) {
  if (!illegal_object_chars)
    return false;
  return name.find_first_of(illegal_object_chars) != std::string::npos;
}

/******************************************************************/
/************************* FILE FUNCTIONS *************************/
/******************************************************************/

/**
 *  Set the close-on-exec flag on the file descriptor.
 *
 *  @param[in] fd The file descriptor to set close on exec.
 *
 *  @return True on succes, otherwise false.
 */
bool set_cloexec(int fd) {
  int flags(0);
  while ((flags = fcntl(fd, F_GETFD)) < 0) {
    if (errno == EINTR)
      continue;
    return false;
  }
  while (fcntl(fd, F_SETFD, flags | FD_CLOEXEC) < 0) {
    if (errno == EINTR)
      continue;
    return false;
  }
  return true;
}

/******************************************************************/
/*********************** CLEANUP FUNCTIONS ************************/
/******************************************************************/

/**
 *  Do some cleanup before we exit.
 */
void cleanup() {
  // Unload modules.
  if (!test_scheduling) {
    checks::checker::deinit();
    /* Before stopping, we stop all the connectors that are not already
     * finished. */
    for (auto& c : commands::connector::connectors)
      c.second->stop_connector();

    /* Before stopping, we destroy all the running checks that are not already
     * finished. */
    com::centreon::engine::commands::command::commands.clear();

    neb_free_callback_list();
    neb_unload_all_modules(NEBMODULE_FORCE_UNLOAD, sigshutdown
                                                       ? NEBMODULE_NEB_SHUTDOWN
                                                       : NEBMODULE_NEB_RESTART);
    neb_free_module_list();
    neb_deinit_modules();
  }

  // Free all allocated memory.
  free_memory();
}

/**
 *  Free the memory allocated to the linked lists.
 */
void free_memory() {
  // Free memory allocated to downtimes.
  downtime_manager::instance().clear_scheduled_downtimes();
}

// Thread-safe initialization guard
std::once_flag symbolizer_initialized;

void ensure_symbolizer_initialized(const char* program_name) {
  std::call_once(symbolizer_initialized, [program_name]() {
    absl::InitializeSymbolizer(program_name);
  });
}

/**
 * @brief Captures the current stack trace and returns it as a formatted string.
 *
 * This function captures the current call stack and attempts to symbolize each
 * frame to provide readable function names. If symbolization fails for a frame,
 * the raw address is displayed instead.
 *
 * @param max_depth Maximum number of stack frames to capture (default: 64)
 * @return std::string A formatted string containing the stack trace, with each
 * frame on a separate line in the format "#<frame_number> <function_name>"
 *
 * @note This function skips its own frame in the stack trace output.
 * @note Requires absl::InitializeSymbolizer() to be called for proper
 * symbolization.
 *
 * @example
 * @code
 * void debug_function() {
 *     std::cout << get_stack_trace() << std::endl;
 * }
 * @endcode
 */
std::string get_stack_trace(int max_depth) {
  // Ensure symbolizer is initialized (safe to call multiple times)
  ensure_symbolizer_initialized("program");

  // Array to store stack frame addresses
  void* stack[max_depth];

  // Capture the stack trace, skipping this function (skip_count = 1)
  int depth = absl::GetStackTrace(stack, max_depth, 1);

  std::string result;
  result.reserve(1024);  // Reserve space to avoid reallocations

  for (int i = 0; i < depth; ++i) {
    char symbol[1024];

    // Try to symbolize the address to get function name
    if (absl::Symbolize(stack[i], symbol, sizeof(symbol))) {
      absl::StrAppendFormat(&result, "#%d %s\n", i, symbol);
    } else {
      // If symbolization fails, display the raw address
      absl::StrAppendFormat(&result, "#%d <unknown> [%p]\n", i, stack[i]);
    }
  }

  return result;
}

void update_rpc_server(const std::string& listen_address, uint16_t port) {
  if (rpc) {
    rpc->shutdown();
    rpc.reset();
  }
  rpc = std::make_unique<enginerpc>(listen_address, port);
}

void stop_rpc_server() {
  if (rpc) {
    rpc->shutdown();
    rpc.reset();
  }
}
