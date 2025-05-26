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
