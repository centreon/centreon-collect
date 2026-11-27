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

#ifdef _WIN32
  /**
   * @brief Takes ownership of job_created, the job object that holds the
   * process and its descendants.
   */
  boost_process(boost::process::v2::basic_process<
                    asio::io_context::executor_type>&& proc_created,
                HANDLE job_created)
      : proc(std::move(proc_created)), job(job_created) {}

  /**
   * @brief The job is created with JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE: closing
   * it kills the descendants still running.
   */
  ~boost_process() {
    if (job) {
      CloseHandle(job);
    }
  }
#endif

  boost_process(const boost_process&) = delete;
  boost_process& operator=(const boost_process&) = delete;

  boost::process::v2::basic_process<asio::io_context::executor_type> proc;

#ifdef _WIN32
  /**
   * Windows does not kill the children of a killed process. The child process
   * is started in this job object, and its descendants inherit it, so that
   * they can all be killed with it. nullptr if the job could not be created.
   */
  HANDLE job = nullptr;
#endif
};
}  // namespace com::centreon::common::detail

#endif
