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

#ifndef CENTREON_AGENT_NATIVE_CHECK_MEMORY_HH
#define CENTREON_AGENT_NATIVE_CHECK_MEMORY_HH

#include "native_check_memory_base.hh"

struct _PERFORMANCE_INFORMATION;

namespace com::centreon::agent {
namespace check_memory_detail {

enum e_metric : unsigned {
  phys_total,
  phys_free,
  phys_used,
  swap_total,
  swap_free,
  swap_used,
  virtual_total,
  virtual_free,
  virtual_used,
  nb_metric
};

/**
 * @brief this class compute a measure of memory metrics and store in _metrics
 * member
 *
 */
class w_memory_info
    : public memory_info<check_memory_detail::e_metric::nb_metric> {
 public:
  enum output_flags : unsigned { dump_swap = 1, dump_virtual };

  w_memory_info();
  w_memory_info(const MEMORYSTATUSEX& mem_status,
                const struct _PERFORMANCE_INFORMATION& perf_mem_status);
  void init(const MEMORYSTATUSEX& mem_status,
            const struct _PERFORMANCE_INFORMATION& perf_mem_status);

  void dump_to_output(std::string* output, unsigned flags) const override;
};

}  // namespace check_memory_detail

/**
 * @brief native final check object
 *
 */
class check_memory
    : public check_memory_base<check_memory_detail::e_metric::nb_metric> {
 public:
  check_memory(const std::shared_ptr<asio::io_context>& io_context,
               const std::shared_ptr<spdlog::logger>& logger,
               time_point first_start_expected,
               duration check_interval,
               const std::string& serv,
               const std::string& cmd_name,
               const std::string& cmd_line,
               const rapidjson::Value& args,
               const engine_to_agent_request_ptr& cnf,
               check::completion_handler&& handler);

  std::shared_ptr<check_memory_detail::memory_info<
      check_memory_detail::e_metric::nb_metric>>
  measure() const override;
};

}  // namespace com::centreon::agent

#endif