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
#include "com/centreon/exceptions/msg_fmt.hh"

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

/**
 * @brief Default constructor.
 *
 * Initialises all file descriptors to 0 and sets both close flags to true so
 * that the destructor will close whichever sides have not been explicitly
 * transferred to caller ownership.
 */
fork_pipes::fork_pipes()
    : _son_fds{0,0,0,}, _father_fds{0,0,0} ,_has_to_close_son_side(true), _has_to_close_father_side(true) {
}

/**
 * @brief Destructor.
 *
 * Closes the child-side and/or parent-side file descriptors according to
 * the flags set by set_has_to_close_son_side() and
 * set_has_to_close_father_side(). Flags that have been cleared (ownership
 * transferred to the caller) are left untouched.
 */
fork_pipes::~fork_pipes() {
  if (_has_to_close_son_side) {
    close_son_side();
  }
  if (_has_to_close_father_side) {
    close_father_side();
  }
}

/**
 * @brief Create the three pipe pairs (stdin, stdout, stderr).
 *
 * For each standard stream, a pipe() call produces two ends that are
 * assigned as follows:
 *   - stdin:  child reads from _son_fds[0],  parent writes to _father_fds[0]
 *   - stdout: child writes to _son_fds[1],   parent reads from _father_fds[1]
 *   - stderr: child writes to _son_fds[2],   parent reads from _father_fds[2]
 *
 * Throws exceptions::msg_fmt on pipe() failure.
 */
void fork_pipes::init() {
  int fds[2];
  if (pipe(fds)) {
    throw exceptions::msg_fmt("{}", strerror(errno));
  }
  _son_fds[STDIN_FILENO] = fds[0];
  _father_fds[STDIN_FILENO] = fds[1];
  if (pipe(fds)) {
    throw exceptions::msg_fmt("{}", strerror(errno));
  }
  _son_fds[STDOUT_FILENO] = fds[1];
  _father_fds[STDOUT_FILENO] = fds[0];
  if (pipe(fds)) {
    throw exceptions::msg_fmt("{}", strerror(errno));
  }
  _son_fds[STDERR_FILENO] = fds[1];
  _father_fds[STDERR_FILENO] = fds[0];
}

/**
 * @brief Close all child-side (son) file descriptors.
 *
 * Iterates over the three child-side FDs and closes any that are > 0
 * (descriptors initialised to 0 are considered not yet opened).
 */
void fork_pipes::close_son_side() {
  for (int* fd = _son_fds; fd < _son_fds + 3; ++fd) {
    if (*fd > 0) {
      ::close(*fd);
    }
  }
}

/**
 * @brief Close all parent-side (father) file descriptors.
 *
 * Iterates over the three parent-side FDs and closes any that are > 0
 * (descriptors initialised to 0 are considered not yet opened).
 */
void fork_pipes::close_father_side() {
  for (int* fd = _father_fds; fd < _father_fds + 3; ++fd) {
    if (*fd > 0) {
      ::close(*fd);
    }
  }
}

/**
 * @brief Redirect child-side pipe ends to the real stdin/stdout/stderr.
 *
 * Must be called from the child process after fork(), before the Perl script
 * is executed. Uses dup2() to replace file descriptors 0, 1, and 2 with the
 * corresponding child-side pipe ends so that standard I/O of the script flows
 * through the pipes instead of the terminal.
 *
 * @return true on success, false if any dup2() call fails (error logged to
 *         stderr).
 */
bool fork_pipes::dup_son_fd_to_std() {
  if (::dup2(_son_fds[STDIN_FILENO], STDIN_FILENO) < 0) {
    std::cerr << "stdin dup2 error: " << strerror(errno) << std::endl;
    return false;
  }
  if (::dup2(_son_fds[STDOUT_FILENO], STDOUT_FILENO) < 0) {
    std::cerr << "stdout dup2 error: " << strerror(errno) << std::endl;
    return false;
  }
  if (::dup2(_son_fds[STDERR_FILENO], STDERR_FILENO) < 0) {
    std::cerr << "stderr dup2 error: " << strerror(errno) << std::endl;
    return false;
  }
  return true;
}

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
                         fork_pipes& pipes,
                         const shared_io_context& io_context) {
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
        throw exceptions::msg_fmt("could not compile Perl script {}", file);
    }
    SPAGAIN;
    handle = POPs;
    if (SvTRUE(ERRSV))
      throw exceptions::msg_fmt("Embedded Perl error: {}", SvPV_nolen(ERRSV));

    // Insert in parsed file list.
    _parsed.insert(std::make_pair(file, handle));
  }
  // Already parsed.
  else
    handle = it->second;

  io_context->notify_fork(asio::io_context::fork_prepare);
  log::core()->flush();
  // Execute Perl file.
  pid_t child(fork());
  if (child > 0) {  // Parent
    io_context->notify_fork(asio::io_context::fork_parent);
    pipes.close_son_side();
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
    pipes.close_father_side();
    if (!pipes.dup_son_fd_to_std()) {
      std::cerr << "dup2 error: " << strerror(errno) << std::endl;
      pipes.close_son_side();
      exit(3);
    }

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
    pipes.close_son_side();
    pipes.close_father_side();
    throw exceptions::msg_fmt("{}", msg);
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
      throw exceptions::msg_fmt("could not create temporary file: {}", msg);
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
          throw exceptions::msg_fmt("could not write embedded script: {}", msg);
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
    throw exceptions::msg_fmt("could not allocate Perl interpreter");
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
    throw exceptions::msg_fmt("could not parse embedded Perl script");
  }
  PL_exit_flags |= PERL_EXIT_DESTRUCT_END;
  perl_run(my_perl);
}
