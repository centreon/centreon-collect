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

#ifndef CENTREON_AGENT_CHECK_SCHED_HH
#define CENTREON_AGENT_CHECK_SCHED_HH

#include <comdef.h>
#include <taskschd.h>
#include <wrl/client.h>

#include "check.hh"
#include "com/centreon/common/rapidjson_helper.hh"
#include "filter.hh"

using Microsoft::WRL::ComPtr;

namespace com::centreon::agent {

/**
 * @brief time_point with his string display
 *
 */
struct DateInfo {
  std::chrono::system_clock::time_point timestamp;
  std::string formatted;
};

/**
 * @brief all datas of a task
 *
 */
struct tasksched_data : public testable {
  std::string name;
  std::string folder;
  bool enabled;
  DateInfo last_run;
  DateInfo next_run;
  long exit_code{0};
  TASK_STATE state;
  std::string author;
  std::string description;
  long number_missed_runs{0};
  long duration_last_run{0};
};

/**
 * @brief Check for scheduled tasks.
 * This check retrieves all scheduled tasks and checks their status against
 * specified thresholds. It can filter tasks based on various criteria and
 * provides detailed information about each task.
 */
class check_sched : public check {
  absl::flat_hash_set<std::string> _exclude_tasks;

  std::string _filter_tasks;
  std::string _output_syntax;
  std::string _task_detail_syntax;
  std::string _ok_syntax;
  std::string _warning_status;
  std::string _critical_status;
  unsigned _warning_threshold_count{0};
  unsigned _critical_threshold_count{0};
  bool _verbose{false};
  int _task_count{0};

  absl::btree_set<std::string> _ok_list;
  absl::btree_set<std::string> _warning_list;
  absl::btree_set<std::string> _critical_list;

  absl::flat_hash_map<std::string, tasksched_data> _tasks;

  std::function<void(filter*)> _checker_builder;

  std::unique_ptr<filters::filter_combinator> _task_filter;
  std::unique_ptr<filters::filter_combinator> _warning_rules_filter;
  std::unique_ptr<filters::filter_combinator> _critical_rules_filter;

  ComPtr<ITaskService> _service_ptr;
  ComPtr<ITaskFolder> _root_folder_ptr;

  void _enumerate_tasks(ComPtr<ITaskFolder> folder);
  void _get_task_info(ComPtr<IRegisteredTask> task);
  void _build_checker();

  void _connect_to_sched();
  void _process_exclude_task(const std::string_view& param);
  void _calc_output_format();
  void _print_format(std::string* output, e_status status);

 public:
  check_sched(const std::shared_ptr<asio::io_context>& io_context,
              const std::shared_ptr<spdlog::logger>& logger,
              time_point first_start_expected,
              duration check_interval,
              const std::string& serv,
              const std::string& cmd_name,
              const std::string& cmd_line,
              const rapidjson::Value& args,
              const engine_to_agent_request_ptr& cnf,
              check::completion_handler&& handler,
              const checks_statistics::pointer& stat);

  static void help(std::ostream& help_stream);

  void start_check(const duration& timeout) override;

  e_status compute(std::string* output,
                   std::list<com::centreon::common::perfdata>* perfs);

  // for testing purposes
  absl::flat_hash_map<std::string, tasksched_data>& get_mutable_tasks() {
    return _tasks;
  }
  const std::unique_ptr<filters::filter_combinator>& get_task_filter() {
    return std::move(_task_filter);
  }
  const std::unique_ptr<filters::filter_combinator>&
  get_warning_rules_filter() {
    return std::move(_warning_rules_filter);
  }
  absl::flat_hash_set<std::string> get_exclude_tasks() const {
    return _exclude_tasks;
  }

  void apply_filter();
};
}  // namespace com::centreon::agent
#endif