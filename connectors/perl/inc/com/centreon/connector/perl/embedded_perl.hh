/*
** Copyright 2022 Centreon
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

#ifndef CCCP_EMBEDDED_PERL_HH
#define CCCP_EMBEDDED_PERL_HH

#include <EXTERN.h>
#include <perl.h>
#include <sys/types.h>

// Global Perl interpreter.
extern PerlInterpreter* my_perl;

namespace com::centreon::connector::perl {

/**
 * @class fork_pipes embedded_perl.hh
 * "com/centreon/connector/perl/embedded_perl.hh"
 * @brief Manages pipe file descriptors across a fork().
 *
 * Creates and owns three pipe pairs (stdin, stdout, stderr) shared between
 * a parent process (father side) and its forked child (son side).
 * Each side can be closed independently; the destructor closes any side
 * whose close flag is still set.
 */
class fork_pipes {
  /** File descriptors seen by the child process: [stdin, stdout, stderr]. */
  int _son_fds[3];
  /** File descriptors seen by the parent process: [stdin, stdout, stderr]. */
  int _father_fds[3];
  /** Whether the destructor should close the child-side descriptors. */
  bool _has_to_close_son_side;
  /** Whether the destructor should close the parent-side descriptors. */
  bool _has_to_close_father_side;

 public:
  fork_pipes();
  ~fork_pipes();

  /** Close all child-side (son) file descriptors. */
  void close_son_side();
  /** Close all parent-side (father) file descriptors. */
  void close_father_side();

  /**
   * Create the three pipe pairs (stdin, stdout, stderr).
   * Must be called before fork(). Throws on pipe() failure.
   */
  void init();

  /**
   * Control whether the child-side descriptors are closed by the destructor.
   * @param has_to_close  true to close automatically, false to leave open.
   */
  void set_has_to_close_son_side(bool has_to_close) {
    _has_to_close_son_side = has_to_close;
  }

  /**
   * Control whether the parent-side descriptors are closed by the destructor.
   * @param has_to_close  true to close automatically, false to leave open.
   */
  void set_has_to_close_father_side(bool has_to_close) {
    _has_to_close_father_side = has_to_close;
  }

  /**
   * Redirect the child-side pipe ends to the actual stdin/stdout/stderr
   * file descriptors via dup2(). Must be called from the child process after
   * fork(). Returns false and logs an error if any dup2() call fails.
   */
  bool dup_son_fd_to_std();

  /** @return Pointer to the three child-side file descriptors [stdin, stdout,
   * stderr]. */
  const int* get_son_fds() const { return _son_fds; }
  /** @return Pointer to the three parent-side file descriptors [stdin, stdout,
   * stderr]. */
  const int* get_father_fds() const { return _father_fds; }
};

/**
 *  @class embedded_perl embedded_perl.hh
 * "com/centreon/connector/perl/embedded_perl.hh"
 *  @brief Embedded Perl interpreter.
 *
 *  Embedded Perl interpreter wrapped in a singleton.
 */
class embedded_perl {
 public:
  ~embedded_perl();
  static embedded_perl& instance();
  static void load(int argc, char** argv, char** env, char const* code = NULL);
  pid_t run(std::string const& cmd,
            fork_pipes& pipes,
            const shared_io_context& io_context);
  static void unload();

 private:
  using cmd_to_perl_map = absl::flat_hash_map<std::string, SV*>;

  embedded_perl(int argc, char** argv, char** env, char const* code = NULL);
  embedded_perl(embedded_perl const& ep);
  embedded_perl& operator=(embedded_perl const& ep);

  cmd_to_perl_map _parsed;
  static char const* const _script;
  pid_t _self;
  char** _argv;
};

}  // namespace com::centreon::connector::perl

#endif  // !CCCP_EMBEDDED_PERL_HH
