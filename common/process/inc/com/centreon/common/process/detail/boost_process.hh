/**
 * Copyright 2026 Centreon
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

#ifndef CENTREON_COMMON_BOOST_PROCESS_HH
#define CENTREON_COMMON_BOOST_PROCESS_HH
namespace com::centreon::common::detail {

/**
 * @brief The only goal of this struct is to hide boost::process implementation
 * So, you will find a shared_ptr<boost_process> attribute in process class
 * I don't know why, but you can't define a unique_ptr of unknown struct in a
 * class attribute so, we use raw pointer instead
 *
 * Only include it in cc files
 */
struct boost_process {
  boost_process(
      boost::process::v2::basic_process<asio::io_context::executor_type>&&
          proc_created)
      : proc(std::move(proc_created)) {}

  boost_process(const boost_process&) = delete;
  boost_process& operator=(const boost_process&) = delete;

  boost::process::v2::basic_process<asio::io_context::executor_type> proc;
};
}  // namespace com::centreon::common::detail

#endif
