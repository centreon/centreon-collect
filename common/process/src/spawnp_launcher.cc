/**
 * Copyright 2024 Centreon
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

#include <linux/close_range.h>
#include <spawn.h>
#include <sys/syscall.h>

#include "com/centreon/common/process/detail/spawnp_launcher.hh"
#include "com/centreon/exceptions/msg_fmt.hh"

namespace com::centreon::common::detail {
/**
 * @brief spawn attribute encapsulation
 *
 */
struct spawn_attr {
  posix_spawnattr_t attr;

  spawn_attr() { posix_spawnattr_init(&attr); }

  ~spawn_attr() { posix_spawnattr_destroy(&attr); }
};

struct spawn_file_action {
  posix_spawn_file_actions_t actions;
  spawn_file_action() { posix_spawn_file_actions_init(&actions); }

  ~spawn_file_action() { posix_spawn_file_actions_destroy(&actions); }
};

}  // namespace com::centreon::common::detail

/**
 * @brief create a child process by calling spawnp
 *
 * @param io_context
 * @param args process_args class that contains exe path and arguments
 * @param use_setpgid true if we want a setpgid
 * @param stdin_fd
 * @param stdout_fd
 * @param stderr_fd
 * @param envp array of strings ended with a nullptr
 * @return boost::process::v2::basic_process<asio::io_context::executor_type>
 */
boost::process::v2::basic_process<asio::io_context::executor_type>
com::centreon::common::detail::spawnp(asio::io_context& io_context,
                                      const process_args::pointer& args,
                                      bool use_setpgid,
                                      int stdin_fd,
                                      int stdout_fd,
                                      int stderr_fd,
                                      char* const envp[]) {
  spawn_attr attr;
  spawn_file_action file_action;
  int ret;

  if (use_setpgid) {
    int ret = posix_spawnattr_setflags(&attr.attr, POSIX_SPAWN_SETPGROUP);
    if (!ret) {
      posix_spawnattr_setpgroup(&attr.attr, 0);
    }
  }
  if (stdin_fd > 0) {
    ret = posix_spawn_file_actions_adddup2(&file_action.actions, stdin_fd,
                                           STDIN_FILENO);
    if (ret) {
      throw exceptions::msg_fmt("fail to adddup2 stdin: {}", strerror(ret));
    }
  }
  if (stdout_fd > 0) {
    ret = posix_spawn_file_actions_adddup2(&file_action.actions, stdout_fd,
                                           STDOUT_FILENO);
    if (ret) {
      throw exceptions::msg_fmt("fail to adddup2 stdout: {}", strerror(ret));
    }
  }
  if (stderr_fd > 0) {
    ret = posix_spawn_file_actions_adddup2(&file_action.actions, stderr_fd,
                                           STDERR_FILENO);
    if (ret) {
      throw exceptions::msg_fmt("fail to adddup2 stderr: {}", strerror(ret));
    }
  }

  pid_t pid(static_cast<pid_t>(-1));

// child not play with parent fds
#ifdef BOOST_PROCESS_V2_HAS_CLOSE_RANGE
  ::close_range(3, ~0u, CLOSE_RANGE_CLOEXEC);
#else
// alma8 does not provide close_range = > syscall
/* Set the FD_CLOEXEC bit instead of closing the file descriptor. */
#define CLOSE_RANGE_CLOEXEC (1U << 2)
  ::syscall(3, ~0u, CLOSE_RANGE_CLOEXEC);
#endif

  if (posix_spawnp(&pid, args->get_exe_path().c_str(), &file_action.actions,
                   &attr.attr,
                   const_cast<char* const*>(args->get_c_args().data()),
                   envp ? envp : ::environ)) {
    char const* msg(strerror(errno));
    throw exceptions::msg_fmt(msg);
  }

  return boost::process::v2::basic_process<asio::io_context::executor_type>(
      io_context.get_executor(), pid);
}