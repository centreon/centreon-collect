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

#ifndef CENTREON_AGENT_CHECK_FILES_HH
#define CENTREON_AGENT_CHECK_FILES_HH

#include <windows.h>
#include <filesystem>
#include "check.hh"
#include "filter.hh"

namespace fs = std::filesystem;
namespace com::centreon::agent {

/*
 * @brief Metadata for a file.
 *
 * This structure holds various metadata about a file, including its path,
 * name, extension, timestamps, size, number of lines, age since last modified,
 * version, and read time.
 */
struct file_metadata : public testable {
  std::string path;
  std::string name;
  std::string extension;
  fs::file_time_type creation_time;
  fs::file_time_type last_access_time;
  fs::file_time_type last_write_time;
  std::uint64_t size = 0;
  std::uint64_t number_of_lines = 0;
  std::string version;

  file_metadata() = default;
  file_metadata(const file_metadata&) = delete;
  // Delete copy assignment operator
  file_metadata& operator=(const file_metadata&) = delete;

  file_metadata(const std::string& file_path, bool line_count_needed);
};

namespace check_files_detail {

/**
 * @brief Filter class for file metadata.
 *
 * This class is responsible for finding files based on a specified pattern,
 * maximum depth, and applying filters to the files found.
 * It stores metadata about the files in a hash map.
 */
class filter {
  absl::flat_hash_map<std::string, std::unique_ptr<file_metadata>>
      _files_metadata;

  std::shared_ptr<filters::filter_combinator> _file_filter;

  std::string _root_path;
  std::string _pattern;
  int _max_depth;
  bool _line_count_needed;

  filter(const filter&) = delete;
  // Delete copy assignment operator
  filter& operator=(const filter&) = delete;

 public:
  filter(const std::string& root_path,
         const std::string& pattern,
         int max_depth,
         bool line_count_needed,
         std::shared_ptr<filters::filter_combinator> file_filter)
      : _root_path(root_path),
        _pattern(pattern),
        _max_depth(max_depth),
        _line_count_needed(line_count_needed),
        _file_filter(file_filter) {}

  void find_files();
  const absl::flat_hash_map<std::string, std::unique_ptr<file_metadata>>&
  get_files_metadata() const {
    return _files_metadata;
  }
  void clear_files_metadata() { _files_metadata.clear(); }
};

/*
 * @brief Thread class for checking files asynchronously.
 *
 * This class manages a queue of file check requests and processes them
 * asynchronously. It allows for multiple requests to be handled concurrently
 * and provides a completion handler for each request.
 */
class check_files_thread
    : public std::enable_shared_from_this<check_files_thread> {
  using completion_handler = std::function<void(
      const absl::flat_hash_map<std::string, std::unique_ptr<file_metadata>>&)>;

  /* * @brief Data structure to hold asynchronous request data.
   *
   * This structure contains a filter for the request, a completion handler,
   * and a timeout for the request.
   */
  struct async_data {
    std::shared_ptr<filter> request_filter;
    completion_handler handler;
    time_point timeout;
  };

  absl::Mutex _queue_m;
  std::list<async_data> _queue ABSL_GUARDED_BY(_queue_m);
  bool _active = true;
  std::shared_ptr<asio::io_context> _io_context;
  std::shared_ptr<spdlog::logger> _logger;

 public:
  check_files_thread(const std::shared_ptr<asio::io_context>& io_context,
                     const std::shared_ptr<spdlog::logger>& logger)
      : _io_context(io_context), _logger(logger) {}
  void run();
  void kill();

  bool has_to_wait() const ABSL_EXCLUSIVE_LOCKS_REQUIRED(_queue_m) {
    return !_active || !_queue.empty();
  }

  template <class handler_type>
  void async_get_files(const std::shared_ptr<filter>& request_filter,
                       const time_point& timeout,
                       handler_type&& handler);
};
}  // namespace check_files_detail

/**
 * @brief file check for windows
 *
 */
class check_files : public check {
  std::string _root_path;
  std::string _pattern;
  std::string _filter_files;
  std::string _output_syntax;
  std::string _files_detail_syntax;
  std::string _ok_syntax;
  std::string _warning_status;
  std::string _critical_status;
  unsigned _warning_threshold_count{0};
  unsigned _critical_threshold_count{0};
  bool _verbose{false};

  int _max_depth;
  bool _line_count_needed{false};

  std::shared_ptr<check_files_detail::filter> _filter;

  absl::btree_set<std::string> _ok_list;
  absl::btree_set<std::string> _warning_list;
  absl::btree_set<std::string> _critical_list;

  std::function<void(filter*)> _checker_builder;

  std::shared_ptr<filters::filter_combinator> _file_filter;
  std::unique_ptr<filters::filter_combinator> _warning_rules_filter;
  std::unique_ptr<filters::filter_combinator> _critical_rules_filter;

  void _build_checker();
  void _calc_output_format();

  void _print_format(std::string& output, e_status status);

  void _completion_handler(
      unsigned start_check_index,
      const absl::flat_hash_map<std::string, std::unique_ptr<file_metadata>>&
          result);

 public:
  check_files(const std::shared_ptr<asio::io_context>& io_context,
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

  std::shared_ptr<check_files> shared_from_this() {
    return std::static_pointer_cast<check_files>(check::shared_from_this());
  }

  void start_check(const duration& timeout) override;
  static void thread_kill();

  static void help(std::ostream& help_stream);
};

std::string glob_to_regex(std::string_view glob);

}  // namespace com::centreon::agent
#endif
