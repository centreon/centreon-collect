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

#include "com/centreon/engine/utils.hh"

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
#include "com/centreon/engine/downtimes/downtime_manager.hh"
#include "com/centreon/engine/events/loop.hh"
#include "com/centreon/engine/globals.hh"
#include "com/centreon/engine/logging/logger.hh"
#include "com/centreon/engine/macros.hh"
#include "com/centreon/engine/nebmods.hh"
#include "com/centreon/engine/shared.hh"
#include "com/centreon/engine/string.hh"

using namespace com::centreon;
using namespace com::centreon::engine;
using namespace com::centreon::engine::events;
using namespace com::centreon::engine::logging;

/******************************************************************/
/******************** SYSTEM COMMAND FUNCTIONS ********************/
/******************************************************************/

/* executes a system command - used for notifications, event handlers, etc. */
int my_system_r(nagios_macros* mac,
                std::string const& cmd,
                int timeout,
                bool* early_timeout,
                double* exectime,
                std::string& output,
                unsigned int max_output_length) {
  engine_logger(dbg_functions, basic) << "my_system_r()";
  functions_logger->trace("my_system_r()");

  // initialize return variables.
  *early_timeout = false;
  *exectime = 0.0;

  // if no command was passed, return with no error.
  if (cmd.empty()) {
    return service::state_ok;
  }

  engine_logger(dbg_commands, more) << "Running command '" << cmd << "'...";
  SPDLOG_LOGGER_DEBUG(commands_logger, "Running command '{}'...", cmd);

  timeval start_time = timeval();
  timeval end_time = timeval();

  // time to start command.
  gettimeofday(&start_time, nullptr);

  std::shared_ptr<commands::raw_v2> raw_cmd =
      std::make_shared<commands::raw_v2>(g_io_context, "system", cmd);
  commands::result res;
  raw_cmd->run(cmd, *mac, timeout, res);

  end_time.tv_sec = res.end_time.to_seconds();
  end_time.tv_usec = res.end_time.to_useconds() - end_time.tv_sec * 1000000ull;
  *exectime = (res.end_time - res.start_time).to_seconds();
  *early_timeout = res.exit_status == process::timeout;
  if (max_output_length > 0)
    output = res.output.substr(0, max_output_length - 1);
  else
    output = res.output;
  int result(res.exit_code);

  engine_logger(dbg_commands, more)
      << com::centreon::logging::setprecision(3)
      << "Execution time=" << *exectime
      << " sec, early timeout=" << *early_timeout << ", result=" << result
      << ", output=" << output;
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

/* given a "raw" command, return the "expanded" or "whole" command line */
int get_raw_command_line_r(nagios_macros* mac,
                           commands::command* cmd_ptr,
                           std::string const& cmd,
                           std::string& full_command,
                           int macro_options) {
  std::string temp_arg;
  temp_arg.reserve(MAX_COMMAND_BUFFER);
  std::string arg_buffer;
  unsigned int x = 0;
  int escaped = false;

  engine_logger(dbg_functions, basic) << "get_raw_command_line_r()";
  functions_logger->trace("get_raw_command_line_r()");

  /* clear the argv macros */
  clear_argv_macros_r(mac);

  /* make sure we've got all the requirements */
  if (cmd_ptr == nullptr) {
    return ERROR;
  }

  engine_logger(dbg_commands | dbg_checks | dbg_macros, most)
      << "Raw Command Input: " << cmd_ptr->get_command_line();
  SPDLOG_LOGGER_DEBUG(commands_logger, "Raw Command Input: {}",
                      cmd_ptr->get_command_line());

  /* get the full command line */
  full_command = cmd_ptr->get_command_line();

  /* get the command arguments */
  if (!cmd.empty()) {
    std::string::const_iterator arg_iter = cmd.begin();
    /* skip the command name (we're about to get the arguments)... */
    for (; arg_iter != cmd.end() && *arg_iter != '!'; ++arg_iter)
      ;

    /* get each command argument */
    for (x = 0; x < MAX_COMMAND_ARGUMENTS && arg_iter != cmd.end(); ++x) {
      /* get the next argument */
      temp_arg.clear();
      for (++arg_iter; arg_iter != cmd.end(); ++arg_iter) {
        /* backslashes escape */
        if (*arg_iter == '\\' && escaped == false) {
          escaped = true;
          continue;
        }

        /* end of argument */
        if (*arg_iter == '!' && escaped == false)
          break;

        /* normal of escaped char */
        temp_arg.push_back(*arg_iter);

        /* clear escaped flag */
        escaped = false;
      }

      /* ADDED 01/29/04 EG */
      /* process any macros we find in the argument */
      process_macros_r(mac, temp_arg, arg_buffer, macro_options);

      mac->argv[x] = std::move(arg_buffer);
    }
  }

  engine_logger(dbg_commands | dbg_checks | dbg_macros, most)
      << "Expanded Command Output: " << full_command;
  SPDLOG_LOGGER_DEBUG(commands_logger, "Expanded Command Output: {}",
                      full_command);

  return OK;
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
/************************* IPC FUNCTIONS **************************/
/******************************************************************/

/**
 * @brief Parse buffer and fill the three strings given as references:
 *    * short_output
 *    * long_output
 *    * perf_data
 *
 * @param[in] buffer
 * @param[out] short_output
 * @param[out] long_output
 * @param[out] perf_data
 * @param[in] escape_newlines_please To escape new lines in the returned strings
 * @param[in] newlines_are_escaped To consider input newlines as escaped.
 *
 */
void parse_check_output(std::string const& buffer,
                        std::string& short_buffer,
                        std::string& long_buffer,
                        std::string& pd_buffer,
                        bool escape_newlines_please,
                        bool newlines_are_escaped) {
  bool long_pipe{false};
  bool perfdata_already_filled{false};

  bool eof{false};
  std::string line;
  /* pos_line is used to cut a line
   * start_line is the position of the line begin
   * end_line is the position of the line end. */
  size_t start_line{0}, end_line, pos_line;
  int line_number{1};
  while (!eof) {
    if (newlines_are_escaped &&
        (pos_line = buffer.find("\\n", start_line)) != std::string::npos) {
      end_line = pos_line;
      pos_line += 2;
    } else if ((pos_line = buffer.find("\n", start_line)) !=
               std::string::npos) {
      end_line = pos_line;
      pos_line++;
    } else {
      end_line = buffer.size();
      eof = true;
    }
    line = buffer.substr(start_line, end_line - start_line);
    size_t pipe;
    if (!long_pipe)
      pipe = line.find_last_of('|');
    else
      pipe = std::string::npos;

    if (pipe != std::string::npos) {
      end_line = pipe;
      /* Let's trim the output */
      while (end_line > 1 && std::isspace(line[end_line - 1]))
        end_line--;

      /* Let's trim the output */
      pipe++;
      while (pipe < line.size() - 1 && std::isspace(line[pipe]))
        pipe++;

      if (line_number == 1) {
        short_buffer.append(line.substr(0, end_line));
        pd_buffer.append(line.substr(pipe));
        perfdata_already_filled = true;
      } else {
        if (line_number > 2)
          long_buffer.append(escape_newlines_please ? "\\n" : "\n");
        long_buffer.append(line.substr(0, end_line));
        if (perfdata_already_filled)
          pd_buffer.append(" ");
        pd_buffer.append(line.substr(pipe));
        // Now, all new lines contain perfdata.
        long_pipe = true;
      }
    } else {
      /* Let's trim the output */
      end_line = line.size();
      while (end_line > 1 && std::isspace(line[end_line - 1]))
        end_line--;
      line.erase(end_line);
      if (line_number == 1)
        short_buffer.append(line);
      else {
        if (!long_pipe) {
          if (line_number > 2)
            long_buffer.append(escape_newlines_please ? "\\n" : "\n");
          long_buffer.append(line);
        } else {
          if (perfdata_already_filled)
            pd_buffer.append(" ");
          pd_buffer.append(line);
        }
      }
    }
    start_line = pos_line;
    line_number++;
  }
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
bool contains_illegal_object_chars(char const* name) {
  if (!name || !illegal_object_chars)
    return false;
  return strpbrk(name, illegal_object_chars) ? true : false;
}

/* compares strings */
int compare_strings(char* val1a, char* val2a) {
  /* use the compare_hashdata() function */
  return compare_hashdata(val1a, nullptr, val2a, nullptr);
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
  if (!test_scheduling && !verify_config) {
    checks::checker::deinit();
    /* Before stopping, we stop all the connectors that are not already finished. */
    for (auto& c : commands::connector::connectors)
      c.second->stop_connector();

    /* Before stopping, we destroy all the running checks that are not already finished. */
    com::centreon::engine::commands::command::commands.clear();

    neb_free_callback_list();
    neb_unload_all_modules(NEBMODULE_FORCE_UNLOAD, sigshutdown
                                                       ? NEBMODULE_NEB_SHUTDOWN
                                                       : NEBMODULE_NEB_RESTART);
    neb_free_module_list();
    neb_deinit_modules();
  }

  // Free all allocated memory - including macros.
  free_memory(get_global_macros());
}

/**
 *  Free the memory allocated to the linked lists.
 *
 *  @param[in,out] mac Macros.
 */
void free_memory(nagios_macros* mac) {
  // Free memory allocated to comments.
  comment::comments.clear();

  // Free memory allocated to downtimes.
  downtimes::downtime_manager::instance().clear_scheduled_downtimes();

  /*
  ** Free memory associated with macros. It's ok to only free the
  ** volatile ones, as the non-volatile are always free()'d before
  ** assignment if they're set. Doing a full free of them here means
  ** we'll wipe the constant macros when we get a reload or restart
  ** request through the command pipe, or when we receive a SIGHUP.
  */
  clear_volatile_macros_r(mac);
  free_macrox_names();
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
