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

#ifndef CENTREON_COMMON_PROCESS_SPAWNP_LAUNCHER_HH
#define CENTREON_COMMON_PROCESS_SPAWNP_LAUNCHER_HH

#if !defined(BOOST_PROCESS_V2_WINDOWS)
/**
 * we force usage of pidfd_open as SYS_close_range and SYS_pidfd_open are
 * available in alma8 even if glibc wrapper are not
 * SYS_pidfd_open and SYS_close_range as were added in kernel 5.3
 */
#define BOOST_PROCESS_V2_PIDFD_OPEN 1
#define BOOST_PROCESS_V2_HAS_PROCESS_HANDLE 1
#ifndef SYS_pidfd_open
#define SYS_pidfd_open 434
#endif
#ifndef SYS_close_range
#define SYS_close_range 436
#endif
#endif

#include <boost/process/v2/process.hpp>
#include "com/centreon/common/process/process_args.hh"

namespace com::centreon::common::detail {

boost::process::v2::basic_process<asio::io_context::executor_type> spawnp(
    asio::io_context& io_context,
    const process_args::pointer& args,
    bool use_setpgid,
    int stdin_fd,
    int stdout_fd,
    int stderr_fd,
    char* const envp[]);

}  // namespace com::centreon::common::detail

#endif
