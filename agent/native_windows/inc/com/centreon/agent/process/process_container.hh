/**
 * Copyright 2025 Centreon
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

#ifndef CENTREON_AGENT_CHECK_PROCESS_CONTAINER_HH
#define CENTREON_AGENT_CHECK_PROCESS_CONTAINER_HH

#include "check.hh"
#include "filter.hh"
#include "process_data.hh"
#include "process_filter.hh"

namespace com::centreon::agent::process {

/**
 * @brief process_container is used to store process information
 * It contains also process filters
 * In this, you will find two type of filters.
 * _warning_filter and _critical_filter will be applied to each process in order
 * to store process in _ok_processes, _warning_processes and _critical_processes
 * _container_warning_filter and _container_critical_filter will be applied to
 * the container in order to set the status of the container according to the
 * number of processes in _ok_processes, _warning_processes and
 * _critical_processes
 */
class container : public testable {
 public:
  using ok_process_cont = std::vector<DWORD>;
  using problem_process_cont = std::vector<process_data>;

 private:
  using process_container = absl::flat_hash_map<DWORD /*pid*/, process_data>;

  std::unique_ptr<process_filter> _filter;
  std::unique_ptr<process_filter> _exclude_filter;
  std::unique_ptr<process_filter> _warning_filter;
  std::unique_ptr<process_filter> _critical_filter;
  std::unique_ptr<filters::filter_combinator> _container_warning_filter;
  std::unique_ptr<filters::filter_combinator> _container_critical_filter;
  unsigned _needed_output_fields;

  std::shared_ptr<spdlog::logger> _logger;

  std::unique_ptr<DWORD[]> _enumerate_buffer;
  size_t _enumerate_buffer_size;

 protected:
  ok_process_cont _ok_processes;
  problem_process_cont _warning_processes;
  problem_process_cont _critical_processes;

 public:
  container(const std::string_view& filter_str,
            const std::string_view& exclude_filter_str,
            const std::string_view& warning_filter_str,
            const std::string_view& critical_filter_str,
            unsigned needed_output_fields,
            const std::shared_ptr<spdlog::logger>& logger);

  void refresh();
  const ok_process_cont& get_ok_processes() const { return _ok_processes; }

  const problem_process_cont& get_warning_processes() const {
    return _warning_processes;
  }

  const problem_process_cont& get_critical_processes() const {
    return _critical_processes;
  }

  bool empty() const {
    return _ok_processes.empty() && _warning_processes.empty() &&
           _critical_processes.empty();
  }

  e_status check_container() const;
};

}  // namespace com::centreon::agent::process

#endif
