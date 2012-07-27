/*
** Copyright 2011-2012 Merethis
**
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

#include <iostream>
#include <memory>
#include <stdlib.h>
#include <unistd.h>
#include <EXTERN.h>
#include <perl.h>
#include "com/centreon/connector/perl/embedded_perl.hh"
#include "com/centreon/exceptions/basic.hh"
#include "com/centreon/logging/logger.hh"

using namespace com::centreon;
using namespace com::centreon::connector::perl;

// Temporary script path.
#define SCRIPT_PATH "/tmp/centreon_connector_perl.XXXXXX"

// Embedded Perl instance.
static std::auto_ptr<embedded_perl> _instance;
// Perl interpreter object.
PerlInterpreter* my_perl(NULL);

// Allow module loading.
EXTERN_C void xs_init(pTHX);

/**************************************
*                                     *
*           Public Methods            *
*                                     *
**************************************/

/**
 *  Embedded Perl destructor.
 */
embedded_perl::~embedded_perl() {
  // Clean only if within parent process.
  if (_self == getpid()) {
    // Clean Perl interpreter.
    logging::info(logging::low) << "cleaning up Embedded Perl";
    if (my_perl) {
      PL_perl_destruct_level = 1;
      perl_destruct(my_perl);
      perl_free(my_perl);
      PERL_SYS_TERM();
    }
  }
  return ;
}

/**
 *  Get instance.
 *
 *  @return Embedded Perl instance.
 */
embedded_perl& embedded_perl::instance() {
  return (*_instance);
}

/**
 *  Load Embedded Perl.
 *
 *  @param[in] argc Argument count.
 *  @param[in] argv Argument values.
 *  @param[in] env  Program environment.
 */
void embedded_perl::load(int* argc, char*** argv, char*** env) {
  unload();
  _instance.reset(new embedded_perl(argc, argv, env));
  return ;
}

/**
 *  Run a Perl script.
 *
 *  @param[in]  cmd Command to execute.
 *  @param[out] fds Process' file descriptors.
 *
 *  @return Process ID.
 */
pid_t embedded_perl::run(std::string const& cmd, int fds[3]) {
  // Check arguments.
  if (!fds)
    throw (basic_error() << "cannot run Perl script without " \
             "fetching process' descriptors");

  // Extract arguments.
  size_t pos(cmd.find(' '));
  std::string args;
  std::string file;
  if (pos != std::string::npos) {
    file = cmd.substr(0, pos);
    args = cmd.substr(pos + 1);
  }
  else
    file = cmd;
  logging::debug(logging::medium)
    << "command " << cmd << "\n"
    << "  - file " << file << "\n"
    << "  - args " << args;

  // Compile Perl file.
  dSP;
  {
    logging::debug(logging::medium) << "parsing file " << file;
    char const* argv[3];
    argv[0] = file.c_str();
    argv[1] = "0";
    argv[2] = NULL;
    if (call_argv("Embed::Persistent::eval_file",
          G_EVAL | G_SCALAR,
          (char**)argv)
        != 1)
      throw (basic_error() << "could not compile Perl script " << file);
  }
  SPAGAIN;
  SV* handle(POPs);
  if (SvTRUE(ERRSV))
    throw (basic_error() << "Embedded Perl error: "
           << SvPV_nolen(ERRSV));

  // Open pipes.
  int in_pipe[2];
  int err_pipe[2];
  int out_pipe[2];
  if (pipe(in_pipe)) {
    char const* msg(strerror(errno));
    throw (basic_error() << msg);
  }
  else if (pipe(err_pipe)) {
    char const* msg(strerror(errno));
    close(in_pipe[0]);
    close(in_pipe[1]);
    throw (basic_error() << msg);
  }
  if (pipe(out_pipe)) {
    char const* msg(strerror(errno));
    close(in_pipe[0]);
    close(in_pipe[1]);
    close(err_pipe[0]);
    close(err_pipe[1]);
    throw (basic_error() << msg);
  }

  // Execute Perl file.
  pid_t child(fork());
  if (child > 0) { // Parent
    close(in_pipe[0]);
    close(err_pipe[1]);
    close(out_pipe[1]);
    fds[0] = in_pipe[1];
    fds[1] = out_pipe[0];
    fds[2] = err_pipe[0];
  }
  else if (!child) { // Child
    // Setup process.
    close(in_pipe[1]);
    close(err_pipe[0]);
    close(out_pipe[0]);
    if (dup2(in_pipe[0], STDIN_FILENO) < 0) {
      char const* msg(strerror(errno));
      std::cerr << "dup2 error: " << msg << std::endl;
      close(in_pipe[0]);
      close(err_pipe[1]);
      close(out_pipe[1]);
      exit(3);
    }
    close(in_pipe[0]);
    if (dup2(err_pipe[1], STDERR_FILENO) < 0) {
      char const* msg(strerror(errno));
      std::cerr << "dup2 error: " << msg << std::endl;
      close(err_pipe[1]);
      close(out_pipe[1]);
      exit(3);
    }
    close(err_pipe[1]);
    if (dup2(out_pipe[1], STDOUT_FILENO) < 0) {
      char const* msg(strerror(errno));
      std::cerr << "dup2 error: " << msg << std::endl;
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
    abort();
  }

  return (child);
}

/**
 *  Unload Embedded Perl.
 */
void embedded_perl::unload() {
  _instance.reset();
  return ;
}

/**************************************
*                                     *
*           Private Methods           *
*                                     *
**************************************/

/**
 *  Default constructor.
 *
 *  @param[in] argc Argument count.
 *  @param[in] argv Argument values.
 *  @param[in] env  Program environment.
 */
embedded_perl::embedded_perl(int* argc, char*** argv, char*** env) {
  // Do not warn if unused by PERL_SYS_INIT3 macro.
  (void)argc;
  (void)argv;
  (void)env;

  // Set original PID.
  _self = getpid();
  logging::debug(logging::high) << "self PID is " << _self;

  // Temporary script path.
  char script_path[] = SCRIPT_PATH;
  {
    // Open embedded script.
    int script_fd(mkstemp(script_path));
    if (script_fd < 0) {
      char const* msg(strerror(errno));
      throw (basic_error()
             << "could not create temporary file: " << msg);
    }
    logging::info(logging::high)
      << "temporary script path is " << script_path;

    // Write embedded script.
    char const* data(_script);
    size_t len(strlen(data));
    while (len > 0) {
      ssize_t wb(write(script_fd, data, len));
      if (wb <= 0) {
        char const* msg(strerror(errno));
        close(script_fd);
        unlink(script_path);
        throw (basic_error()
               << "could not write embedded script: " << msg);
      }
      len -= wb;
      data += wb;
    }
    fsync(script_fd);
    close(script_fd);
  }

  // Initialize Perl interpreter.
  logging::info(logging::low) << "loading Embedded Perl interpreter";
  PERL_SYS_INIT3(argc, argv, env);
  if (!(my_perl = perl_alloc()))
    throw (basic_error() << "could not allocate Perl interpreter");
  perl_construct(my_perl);
  PL_origalen = 1;
  PL_perl_destruct_level = 1;

  // Parse embedded script.
  char const* embedding[2];
  embedding[0] = "";
  embedding[1] = script_path;
  if (perl_parse(
        my_perl,
        &xs_init,
        sizeof(embedding) / sizeof(*embedding),
        (char**)embedding,
        NULL))
    throw (basic_error() << "could not parse embedded Perl script");
  PL_exit_flags |= PERL_EXIT_DESTRUCT_END;
  perl_run(my_perl);
}

/**
 *  Copy constructor.
 *
 *  @param[in] ep Unused.
 */
embedded_perl::embedded_perl(embedded_perl const& ep) {
  _internal_copy(ep);
}

/**
 *  Assignment operator.
 *
 *  @param[in] ep Unused.
 *
 *  @return This object.
 */
embedded_perl& embedded_perl::operator=(embedded_perl const& ep) {
  _internal_copy(ep);
  return (*this);
}

/**
 *  Copy internal data members.
 *
 *  @param[in] ep Object to copy.
 */
void embedded_perl::_internal_copy(embedded_perl const& ep) {
  (void)ep;
  assert(!"Embedded Perl cannot be copied");
  abort();
  return ;
}
