/*
** Copyright 2011 Merethis
** This file is part of Centreon Connector Perl.
**
** Centreon Connector Perl is free software: you can redistribute it
** and/or modify it under the terms of the GNU Affero General Public
** License as published by the Free Software Foundation, either version
** 3 of the License, or (at your option) any later version.
**
** Centreon Connector Perl is distributed in the hope that it will be
** useful, but WITHOUT ANY WARRANTY; without even the implied warranty
** of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
** Affero General Public License for more details.
**
** You should have received a copy of the GNU Affero General Public
** License along with Centreon Connector Perl. If not, see
** <http://www.gnu.org/licenses/>.
*/

#include <EXTERN.h>
#include <perl.h>
#include <iostream>
#include <memory>
#include <signal.h>
#include <sstream>
#include <sys/select.h>
#include <unistd.h>
#include "com/centreon/connector/perl/embedded.hh"
#include "com/centreon/connector/perl/main_io.hh"
#include "com/centreon/connector/perl/process.hh"
#include "com/centreon/connector/perl/processes.hh"

using namespace com::centreon::connector::perl;

// Perl interpreter object.
static PerlInterpreter* my_perl(NULL);
// Original PID.
static pid_t original_pid;
// Script path.
static char script_path[] = "/tmp/centreon_perl_connector.XXXXXX";

/**
 *  Cleanup handler.
 */
static void cleanup() {
  // Clean only if within parent process.
  if (original_pid == getpid()) {
    // Clean Perl interpreter.
    if (my_perl) {
      PL_perl_destruct_level = 1;
      perl_destruct(my_perl);
      perl_free(my_perl);
      PERL_SYS_TERM();
    }
    // Remove temporary file.
    unlink(script_path);
  }
  return ;
}

/**
 *  Send an error packet back to monitoring engine.
 *
 *  @param[in] cmd_id Command ID.
 *  @param[in] error  Error message.
 */
static void send_check_failed(unsigned long long cmd_id,
                              char const* error) {
  // Forge response packet.
  std::ostringstream packet;
  // Packet ID.
  packet << "3";
  packet.put('\0');
  // Command ID.
  packet << cmd_id;
  packet.put('\0');
  // Executed.
  packet << "0";
  packet.put('\0');
  // Exit code.
  packet << "-1";
  packet.put('\0');
  // End time.
  packet << time(NULL);
  packet.put('\0');
  // Error output.
  packet << (error ? error : "(undefined)");
  packet.put('\0');
  // Standard output.
  packet << "(unknown)";
  for (unsigned int i = 0; i < 4; ++i)
    packet.put('\0');

  // Send response packet.
  main_io::instance().write(packet.str());

  return ;
}

/**
 *  Initialize Perl.
 *
 *  @param[in] argc Argument count.
 *  @param[in] argv Argument values.
 *  @param[in] env  Program environment.
 */
int embedded::init(int* argc, char*** argv, char*** env) {
  // Set original PID.
  original_pid = getpid();

  {
    // Open embedded script.
    int script_fd(mkstemp(script_path));
    if (script_fd < 0) {
      std::cerr << "could not create temporary file: "
                << strerror(errno) << std::endl;
      return (1);
    }

    // Write embedded script.
    char const* data(script);
    size_t len(strlen(data));
    while (len > 0) {
      ssize_t wb(write(script_fd, data, len));
      if (wb <= 0) {
        std::cerr << "could not write embedded script: "
                  << strerror(errno) << std::endl;
        close(script_fd);
        unlink(script_path);
        return (1);
      }
      len -= wb;
      data += wb;
    }
    fsync(script_fd);
    close(script_fd);
  }
  atexit(cleanup);

  // Initialize Perl interpreter.
  PERL_SYS_INIT3(argc, argv, env);
  if (!(my_perl = perl_alloc())) {
    std::cerr << "could not allocate Perl interpreter" << std::endl;
    return (1);
  }
  perl_construct(my_perl);
  PL_origalen = 1;
  PL_perl_destruct_level = 1;

  // Parse embedded script.
  char const* embedding[2];
  embedding[0] = "";
  embedding[1] = script_path;
  if (perl_parse(my_perl,
        NULL,
        sizeof(embedding) / sizeof(*embedding),
        (char**)embedding,
        NULL)) {
    std::cerr << "could not parse embedded Perl script" << std::endl;
    return (1);
  }
  PL_exit_flags |= PERL_EXIT_DESTRUCT_END;
  perl_run(my_perl);

  return (0);
}

/**
 *  Run a command.
 *
 *  @param[in] cmd     Command to run.
 *  @param[in] cmd_id  Command ID.
 *  @param[in] timeout Timeout.
 */
void embedded::run_command(std::string const& cmd,
                           unsigned long long cmd_id,
                           time_t timeout) {
  // Extract arguments.
  std::cerr << "run_command" << std::endl;
  size_t pos(cmd.find(' '));
  std::string args;
  std::string file;
  if (pos != std::string::npos) {
    file = cmd.substr(0, pos);
    args = cmd.substr(pos + 1);
  }
  else
    file = cmd;

  // Compile Perl file.
  dSP;
  {
    char const* argv[3];
    argv[0] = file.c_str();
    argv[1] = "0";
    argv[2] = NULL;
    if (call_argv("Embed::Persistent::eval_file",
          G_EVAL | G_SCALAR,
          (char**)argv)
        != 1) {
      send_check_failed(cmd_id, "could not compile Perl script");
      return ;
    }
  }
  SPAGAIN;
  SV* handle(POPs);
  if (SvTRUE(ERRSV)) {
    send_check_failed(cmd_id, SvPV_nolen(ERRSV));
    return ;
  }

  // Open pipes.
  int err_pipe[2];
  int out_pipe[2];
  if (pipe(err_pipe)) {
    send_check_failed(cmd_id, strerror(errno));
    return ;
  }
  if (pipe(out_pipe)) {
    char const* error(strerror(errno));
    close(err_pipe[0]);
    close(err_pipe[1]);
    send_check_failed(cmd_id, error);
    return ;
  }
#ifndef HAVE_PPOLL
  // Check that FD does not exceed fd_set size.
  if ((err_pipe[0] >= FD_SETSIZE)
      || (out_pipe[0] >= FD_SETSIZE)) {
    close(err_pipe[0]);
    close(err_pipe[1]);
    close(out_pipe[0]);
    close(out_pipe[1]);
    send_check_failed(cmd_id, "FD limit reached");
    return ;
  }
#endif /* !HAVE_PPOLL */

  // Execute Perl file.
  pid_t child(fork());
  if (child > 0) { // Parent
    close(err_pipe[1]);
    close(out_pipe[1]);
    try {
      std::auto_ptr<process> proc(new process(cmd_id,
        out_pipe[0],
        err_pipe[0]));
      proc->timeout(timeout);
      processes::instance()[child] = proc.get();
      proc.release();
    }
    catch (...) {
      kill(SIGKILL, child);
      close(err_pipe[0]);
      close(out_pipe[0]);
      send_check_failed(cmd_id, "could not register child process");
    }
  }
  else if (!child) { // Child
    // Setup process.
    close(err_pipe[0]);
    close(out_pipe[0]);
    if (dup2(err_pipe[1], STDERR_FILENO) < 0) {
      close(err_pipe[1]);
      close(out_pipe[1]);
      exit(3);
    }
    close(err_pipe[1]);
    if (dup2(out_pipe[1], STDOUT_FILENO) < 0) {
      close(out_pipe[1]);
      exit(3);
    }
    close(out_pipe[1]);

    // Run check.
    ENTER;
    SAVETMPS;
    PUSHMARK(SP);
    XPUSHs(sv_2mortal(newSVpv(file.c_str(), 0)));
    XPUSHs(handle);
    XPUSHs(sv_2mortal(newSVpv(args.c_str(), 0)));
    PUTBACK;
    call_pv("Embed::Persistent::run_file", G_DISCARD);
    std::cerr << "error while executing Perl script '" << file << "': "
              << SvPV_nolen(ERRSV) << std::endl;
    exit(3);
  }

  return ;
}
