/**
* Copyright 2022 Centreon
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

#include "com/centreon/connector/perl/embedded_perl.hh"
#include "com/centreon/connector/log.hh"
#include "com/centreon/connector/perl/checks/check.hh"
#include "com/centreon/exceptions/basic.hh"

#include <perl.h>

using namespace com::centreon;
using namespace com::centreon::connector::perl;

// Temporary script path.
#define SCRIPT_PATH "/tmp/centreon_connector_perl.XXXXXX"

// Embedded Perl instance.
static embedded_perl* _instance = nullptr;
// Perl interpreter object.
PerlInterpreter* my_perl(nullptr);

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
    log::core()->info("cleaning up Embedded Perl");
    if (my_perl) {
      PL_perl_destruct_level = 1;
      perl_destruct(my_perl);
      perl_free(my_perl);
      PERL_SYS_TERM();
      my_perl = nullptr;
    }
  }
}

/**
 *  Get instance.
 *
 *  @return Embedded Perl instance.
 */
embedded_perl& embedded_perl::instance() {
  return *_instance;
}

/**
 *  Load Embedded Perl.
 *
 *  @param[in] argc Argument count.
 *  @param[in] argv Argument values.
 *  @param[in] env  Program environment.
 *  @param[in] code Additional code to run by interpreter.
 */
void embedded_perl::load(int argc, char** argv, char** env, char const* code) {
  if (!_instance)
    _instance = new embedded_perl(argc, argv, env, code);
}

/**
 *  Run a Perl script.
 *
 *  @param[in]  cmd Command to execute.
 *  @param[out] fds Process' file descriptors.
 *
 *  @return Process ID.
 */
pid_t embedded_perl::run(std::string const& cmd,
                         int fds[3],
                         const shared_io_context& io_context) {
  // Check arguments.
  if (!fds)
    throw basic_error() << "cannot run Perl script without "
                           "fetching process' descriptors";

  // Extract arguments.
  size_t pos(cmd.find(' '));
  std::string args;
  std::string file;
  if (pos != std::string::npos) {
    file = cmd.substr(0, pos);
    args = cmd.substr(pos + 1);
  } else
    file = cmd;
  log::core()->debug("command {}", cmd);
  log::core()->debug("  - file {}", file);
  log::core()->debug("  - args {}", args);

  // Check if file has already been compiled.
  SV* handle;
  cmd_to_perl_map::const_iterator it(_parsed.find(file));
  dSP;
  if (it == _parsed.end()) {
    // Compile Perl file.
    {
      log::core()->debug("parsing file {}", file);
      char const* argv[3];
      argv[0] = file.c_str();
      argv[1] = "0";
      argv[2] = nullptr;
      if (call_argv("Embed::Persistent::eval_file", G_EVAL | G_SCALAR,
                    (char**)argv) != 1)
        throw basic_error() << "could not compile Perl script " << file;
    }
    SPAGAIN;
    handle = POPs;
    if (SvTRUE(ERRSV))
      throw basic_error() << "Embedded Perl error: " << SvPV_nolen(ERRSV);

    // Insert in parsed file list.
    _parsed.insert(std::make_pair(file, handle));
  }
  // Already parsed.
  else
    handle = it->second;

  // Open pipes.
  int in_pipe[2];
  int err_pipe[2];
  int out_pipe[2];
  if (pipe(in_pipe)) {
    char const* msg(strerror(errno));
    throw basic_error() << msg;
  } else if (pipe(err_pipe)) {
    char const* msg(strerror(errno));
    close(in_pipe[0]);
    close(in_pipe[1]);
    throw basic_error() << msg;
  }
  if (pipe(out_pipe)) {
    char const* msg(strerror(errno));
    close(in_pipe[0]);
    close(in_pipe[1]);
    close(err_pipe[0]);
    close(err_pipe[1]);
    throw basic_error() << msg;
  }

  io_context->notify_fork(asio::io_context::fork_prepare);
  log::core()->flush();
  // Execute Perl file.
  pid_t child(fork());
  if (child > 0) {  // Parent
    io_context->notify_fork(asio::io_context::fork_parent);
    close(in_pipe[0]);
    close(err_pipe[1]);
    close(out_pipe[1]);
    fds[0] = in_pipe[1];
    fds[1] = out_pipe[0];
    fds[2] = err_pipe[0];
  } else if (!child) {  // Child
    io_context->notify_fork(asio::io_context::fork_child);
    unsigned father_process_name_length = strlen(_argv[0]);
    std::string new_process_name("c_");
    new_process_name += basename(file.c_str());
    if (new_process_name.length() > father_process_name_length) {
      new_process_name.resize(father_process_name_length);
    }
    memset(_argv[0], 0, father_process_name_length);
    strcpy(_argv[0], new_process_name.c_str());

    if (log::instance().is_log_to_file()) {
      log::core()->debug("son started pid={}", getpid());
    }
    // default signal handler
    sigset(SIGCHLD, SIG_DFL);
    sigset(SIGTERM, SIG_DFL);
    // close all father fds
    checks::check::close_all_father_fd();
    io_context->stop();
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
    if (log::instance().is_log_to_file()) {
      log::core()->debug("son end pid={} error:{}", getpid(),
                         SvPV_nolen(ERRSV));
    }
    exit(EXIT_SUCCESS);
  } else if (child < 0) {  // Error
    char const* msg(strerror(errno));
    close(in_pipe[0]);
    close(in_pipe[1]);
    close(out_pipe[0]);
    close(out_pipe[1]);
    close(err_pipe[0]);
    close(err_pipe[1]);
    throw basic_error() << msg;
  }

  return child;
}

/**
 *  Unload Embedded Perl.
 */
void embedded_perl::unload() {
  delete _instance;
  _instance = nullptr;
}

/**************************************
 *                                     *
 *           Private Methods           *
 *                                     *
 **************************************/

/**
 *  Constructor.
 *
 *  @param[in] argc Argument count.
 *  @param[in] argv Argument values.
 *  @param[in] env  Program environment.
 *  @param[in] code Additional code to run by interpreter.
 */
embedded_perl::embedded_perl([[maybe_unused]] int argc,
                             char** argv,
                             [[maybe_unused]] char** env,
                             char const* code)
    : _self(getpid()), _argv(argv) {
  // Set original PID.
  log::core()->debug("self PID is {}", _self);

  // Temporary script path.
  char script_path[] = SCRIPT_PATH;
  {
    // Open embedded script.
    int script_fd(mkstemp(script_path));
    if (script_fd < 0) {
      char const* msg(strerror(errno));
      throw basic_error() << "could not create temporary file: " << msg;
    }
    log::core()->info("temporary script path is {}", script_path);

    // Write embedded script.
    std::list<char const*> l;
    l.push_back(_script);
    if (code) {
      l.push_back(code);
      l.push_back("\n");
    }
    for (std::list<char const*>::const_iterator it(l.begin()), end(l.end());
         it != end; ++it) {
      char const* data(*it);
      size_t len(strlen(data));
      while (len > 0) {
        ssize_t wb(write(script_fd, data, len));
        if (wb <= 0) {
          char const* msg(strerror(errno));
          close(script_fd);
          unlink(script_path);
          throw basic_error() << "could not write embedded script: " << msg;
        }
        len -= wb;
        data += wb;
      }
    }
    fsync(script_fd);
    close(script_fd);
  }

  // Initialize Perl interpreter.
  log::core()->info("loading Embedded Perl interpreter");

  if (!(my_perl = perl_alloc())) {
    log::core()->error("could not allocate Perl interpreter");
    throw basic_error() << "could not allocate Perl interpreter";
  }
  perl_construct(my_perl);
  PL_origalen = 1;
  PL_perl_destruct_level = 1;

  // Parse embedded script.
  char const* embedding[2];
  embedding[0] = "";
  embedding[1] = script_path;
  if (perl_parse(my_perl, &xs_init, sizeof(embedding) / sizeof(*embedding),
                 (char**)embedding, nullptr)) {
    log::core()->error("could not parse embedded Perl script");
    throw basic_error() << "could not parse embedded Perl script";
  }
  PL_exit_flags |= PERL_EXIT_DESTRUCT_END;
  perl_run(my_perl);
}
