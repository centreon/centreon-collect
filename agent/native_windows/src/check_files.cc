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

#include "check_files.hh"

#include <boost/interprocess/mapped_region.hpp>
#include "boost/interprocess/file_mapping.hpp"
#include "com/centreon/common/rapidjson_helper.hh"
#include "windows_util.hh"

#include <re2/re2.h>

using namespace com::centreon::agent;

static std::shared_ptr<::check_files_detail::check_files_thread>
    _worker_files_check;
static std::thread* _worker_thread_files_check = nullptr;

/*********************************************************************************************
 *                                          utilities
 *********************************************************************************************/

/**
 * @brief Formats a file size in bytes into a human-readable string with
 * appropriate units.
 *
 * Converts the given size in bytes to a string representation using units such
 * as o (bytes), Ko (kilobytes), Mo (megabytes), Go (gigabytes), or To
 * (terabytes), with two decimal places.
 *
 * @param size The file size in bytes as a double.
 * @return A formatted string representing the size with the most suitable unit.
 */
std::string format_size(double size) {
  const char* units[] = {"o", "Ko", "Mo", "Go", "To"};
  int unit_index = 0;
  while (size >= 1024 && unit_index < 4) {
    size /= 1024.0;
    ++unit_index;
  }
  return fmt::format("{:.2f}{}", size, units[unit_index]);
}

/**
 * @brief Converts a glob pattern to a regular expression string.
 *
 * This function takes a glob pattern (using wildcards like '*' and '?')
 * and converts it into an equivalent regular expression string.
 * The conversion handles the following:
 *   - '*' is converted to ".*" (matches any sequence of characters)
 *   - '?' is converted to "." (matches any single character)
 *   - Regex special characters are escaped to match their literal values.
 *
 * @param glob The input glob pattern as a std::string.
 * @return A std::string containing the equivalent regular expression.
 */
std::string com::centreon::agent::glob_to_regex(std::string_view glob) {
  std::string regex;
  regex.reserve(glob.size() * 2);

  auto flush_literal = [&](std::string_view lit) {
    for (char c : lit) {
      switch (c) {
        case '*':
          regex += ".*";
          break;
        case '?':
          regex += '.';
          break;
        case '.':
        case '+':
        case '(':
        case ')':
        case '|':
        case '^':
        case '$':
        case '\\':
        case '{':
        case '}':
          regex += '\\';
          [[fallthrough]];
        case '[':
        case ']':
        default:
          regex += c;
          break;
      }
    }
  };

  std::size_t i = 0;
  while (i < glob.size()) {
    if (glob[i] != '{') {
      flush_literal(glob.substr(i, 1));
      ++i;
      continue;
    }

    std::size_t brace = i++;  // skip '{'
    int depth = 1;
    std::size_t start = i;
    std::vector<std::string_view> alts;

    while (i < glob.size() && depth) {
      if (glob[i] == '{')
        ++depth;
      else if (glob[i] == '}')
        --depth;

      if ((glob[i] == ',' && depth == 1) || depth == 0) {
        alts.push_back(glob.substr(start, i - start));
        start = i + 1;
      }
      ++i;
    }
    if (depth)  // no matching '}'
      throw exceptions::msg_fmt("unmatched '}' in pattern '{}'", glob);

    // build (?:alt1|alt2)
    regex += "(?:";
    for (std::size_t a = 0; a < alts.size(); ++a) {
      if (a)
        regex += '|';
      regex += com::centreon::agent::glob_to_regex(
          alts[a]);  // recurse: '*' and '?' inside
    }
    regex += ')';
  }
  return regex;
}

/**
 * @brief Retrieves the file version string from the specified file.
 *
 * This function uses Windows version information APIs to extract the
 * "FileVersion" string from the version resource of the given file.
 * If the file does not contain version information, or if any error occurs,
 * an empty string is returned.
 *
 * @param filename The path to the file whose version information is to be
 * retrieved.
 * @return A std::string containing the file version, or an empty string if not
 * available.
 */
std::string get_file_version(const std::string& filename) {
  DWORD handle = 0;
  DWORD size = GetFileVersionInfoSizeA(filename.c_str(), &handle);
  if (size == 0)
    return "";
  std::vector<char> data(size);
  if (!GetFileVersionInfoA(filename.c_str(), handle, size, data.data()))
    return "";
  // Query the language and codepage
  struct LANGANDCODEPAGE {
    WORD wLanguage;
    WORD wCodePage;
  };
  LANGANDCODEPAGE* lpTranslate = nullptr;
  UINT cbTranslate = 0;
  if (!VerQueryValueA(data.data(), "\\VarFileInfo\\Translation",
                      (LPVOID*)&lpTranslate, &cbTranslate) ||
      !lpTranslate)
    return "";
  // Use first translation found
  char subBlock[50];
  snprintf(subBlock, sizeof(subBlock),
           "\\StringFileInfo\\%04x%04x\\FileVersion", lpTranslate[0].wLanguage,
           lpTranslate[0].wCodePage);
  LPVOID lpBuffer = nullptr;
  UINT sizeVer = 0;
  if (VerQueryValueA(data.data(), subBlock, &lpBuffer, &sizeVer) && lpBuffer) {
    return std::string(static_cast<char*>(lpBuffer), sizeVer - 1);
  }
  return "";
}

/**
 * @brief Formats a std::filesystem::file_time_type into a human-readable
 * string.
 *
 * This function converts the given file time to the local time zone and formats
 * it as a string in the "dd/mm/yy HH:MM:SS" format.
 *
 * @param ftime The file time to format, as a std::filesystem::file_time_type.
 * @return A std::string containing the formatted date and time.
 */
std::string format_file_time(const std::filesystem::file_time_type& ftime) {
  using namespace std::chrono;
  auto sctp = time_point_cast<system_clock::duration>(
      ftime - std::filesystem::file_time_type::clock::now() +
      system_clock::now());
  std::time_t cftime = system_clock::to_time_t(sctp);

  std::tm tm_buf;
  localtime_s(&tm_buf, &cftime);
  char buf[30];
  std::strftime(buf, sizeof(buf), "%d/%m/%y %H:%M:%S", &tm_buf);
  return std::string(buf);
}

/*********************************************************************************************
 *                                          file_metadata
 *********************************************************************************************/

/**
 * @brief Constructs a file_metadata object and gathers metadata for the
 * specified file.
 *
 * This constructor initializes the file_metadata object by extracting various
 * properties from the file at the given path, such as its name, extension,
 * timestamps (creation, last access, last write), size, version (for
 * executables and DLLs), and optionally, the number of lines in the file.
 *
 * @param file_path The path to the file whose metadata is to be collected.
 * @param line_count_needed If true, the constructor will count the number of
 * lines in the file.
 *
 * The constructor performs the following actions:
 * - Retrieves file name and extension from the path.
 * - Opens the file using Windows API to obtain creation, access, and write
 * times.
 * - Sets timestamps to minimum values if file access fails.
 * - Retrieves file size using std::filesystem.
 * - Calculates the age since the last modification.
 * - Extracts version information for .exe and .dll files.
 * - If requested, counts the number of lines in the file using memory mapping
 * for efficiency.
 */
file_metadata::file_metadata(const std::string& file_path,
                             bool line_count_needed)
    : path(file_path),
      name(fs::path(file_path).filename().string()),
      extension(fs::path(file_path).extension().string()) {
  HANDLE hFile =
      CreateFile(file_path.c_str(), GENERIC_READ,
                 FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL,
                 OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
  if (hFile != INVALID_HANDLE_VALUE) {
    FILETIME ftCreate, ftAccess, ftWrite;
    if (GetFileTime(hFile, &ftCreate, &ftAccess, &ftWrite)) {
      creation_time = convert_filetime_to_tp(ftCreate);
      last_access_time = convert_filetime_to_tp(ftAccess);
      last_write_time = convert_filetime_to_tp(ftWrite);
    } else {
      creation_time = fs::file_time_type::min();
      last_access_time = fs::file_time_type::min();
      last_write_time = fs::file_time_type::min();
    }
    CloseHandle(hFile);
  } else {
    creation_time = fs::file_time_type::min();
    last_access_time = fs::file_time_type::min();
    last_write_time = fs::file_time_type::min();
  }

  size = fs::file_size(file_path);

  version = "";
  if (extension == ".exe" || extension == ".dll") {
    version = get_file_version(fs::path(file_path).string());
  }

  number_of_lines = 0;
  if (line_count_needed) {
    if (size > 0) {
      boost::interprocess::file_mapping file_map(
          file_path.c_str(), boost::interprocess::read_only);
      boost::interprocess::mapped_region region(file_map,
                                                boost::interprocess::read_only);

      const char* data = static_cast<const char*>(region.get_address());
      std::size_t size = region.get_size();
      const char* cur = data;
      const char* end = data + size;
      while (cur < end) {
        const void* p = std::memchr(cur, '\n', end - cur);
        if (!p)
          break;
        ++number_of_lines;
        cur = static_cast<const char*>(p) + 1;
      }
      // If file does not end with '\n', count the last line
      if (size > 0 && data[size - 1] != '\n') {
        ++number_of_lines;
      }
    }
  }
}

/*********************************************************************************************
 *                                          filter
 *********************************************************************************************/

/**
 * @brief Searches for files matching a specified pattern within a directory
 * tree.
 *
 * This method traverses the directory structure starting from the root path
 * (_root_path), searching recursively for files whose names match the given
 * glob pattern (_pattern). The search respects the maximum depth specified by
 * _max_depth and skips directories where permissions are denied. For each
 * matching file, it creates a file_metadata object, optionally applies an
 * additional file filter (_file_filter), and stores the metadata in the
 * _files_metadata map.
 *
 * @note Files that cannot be processed (e.g., due to access errors) are
 * silently skipped.
 */
void ::check_files_detail::filter::find_files() {
  auto pattern = glob_to_regex(_pattern);

  re2::RE2 regex_pattern(pattern);
  fs::path search_path(_root_path);

  if (!fs::exists(search_path) || !fs::is_directory(search_path)) {
    return;
  }

  fs::recursive_directory_iterator end;

  for (fs::recursive_directory_iterator it(
           search_path,
           std::filesystem::directory_options::skip_permission_denied);
       it != end; ++it) {
    if (_max_depth >= 0 && it.depth() > _max_depth) {
      it.disable_recursion_pending();
      continue;
    }
    if (fs::is_regular_file(*it)) {
      try {
        std::string filename = it->path().filename().string();
        if (re2::RE2::FullMatch(filename, regex_pattern)) {
          auto path_str = it->path().string();
          auto metadata =
              std::make_unique<file_metadata>(path_str, _line_count_needed);
          if (_file_filter && !_file_filter->check(*metadata)) {
            continue;  // skip to next if the data don't match the filter
          }
          _files_metadata[std::move(path_str)] = std::move(metadata);
        }
      } catch (const std::exception& e) {
        continue;  // Skip files that cannot be processed
      }
    }
  }
}
/*********************************************************************************************
 *                                          check_files_thread
 *********************************************************************************************/

/**
 * @brief Main execution loop for the check_files_thread.
 *
 * This method runs in a dedicated thread and manages the processing of file
 * check requests. It keeps the thread alive as long as the _active flag is
 * true. The method waits for new requests to be available in the queue,
 * processes expired requests, and executes file search operations for valid
 * requests. Results are posted asynchronously to the associated io_context for
 * further handling.
 *
 * Thread safety is ensured using absl::MutexLock and condition variables.
 *
 * @note This method should not be called directly; it is intended to be run in
 * a thread context.
 */

void ::check_files_detail::check_files_thread::run() {
  auto keep_object_alive = shared_from_this();

  while (_active) {
    absl::MutexLock l(&_queue_m);
    _queue_m.Await(absl::Condition(this, &check_files_thread::has_to_wait));

    if (!_active) {
      return;
    }

    time_point now = std::chrono::system_clock::now();
    while (!_queue.empty()) {
      if (_queue.begin()->timeout < now) {
        _queue.pop_front();
      } else {
        break;
      }
    }

    if (!_queue.empty()) {
      auto to_execute = _queue.begin();
      // Execute the filter to find files
      auto filter = to_execute->request_filter;
      filter->find_files();
      asio::post(*_io_context, [filter, completion_handler =
                                            std::move(to_execute->handler)]() {
        completion_handler(filter->get_files_metadata());
      });
      _queue.erase(to_execute);
    }
  }
}

void ::check_files_detail::check_files_thread::kill() {
  absl::MutexLock l(&_queue_m);
  _active = false;
}

/**
 * @brief Asynchronously queues a file retrieval request for processing.
 *
 * This templated method adds a new file retrieval task to the internal queue,
 * associating it with a filter, a timeout, and a completion handler.
 * The handler will be invoked upon completion of the file retrieval operation.
 *
 * @tparam handler_type The type of the handler callable to be invoked after
 * file retrieval.
 * @param request_filter Shared pointer to a filter object specifying file
 * selection criteria.
 * @param timeout The time point indicating the deadline for the file retrieval
 * operation.
 * @param handler The callable object to be executed upon completion of the
 * operation.
 *
 * @note This method is thread-safe and acquires a lock on the internal queue
 * mutex.
 */
template <class handler_type>
void ::check_files_detail::check_files_thread::async_get_files(
    const std::shared_ptr<filter>& request_filter,
    const time_point& timeout,
    handler_type&& handler) {
  absl::MutexLock lck(&_queue_m);
  _queue.push_back(
      {request_filter, std::forward<handler_type>(handler), timeout});
}

/*********************************************************************************************
 *                                          check_files
 *********************************************************************************************/

/**
 * @brief Constructs a check_files object to perform file system checks based on
 * provided parameters.
 *
 * This constructor initializes the check_files instance with configuration
 * parameters such as root path, file pattern, depth, output formatting, and
 * filtering options. It parses the arguments from a RapidJSON value, sets up
 * the internal state, and prepares the file checker and output formatting
 * logic.
 *
 * @param io_context Shared pointer to the ASIO I/O context for asynchronous
 * operations.
 * @param logger Shared pointer to the logger instance for logging messages.
 * @param first_start_expected The expected time point for the first check
 * execution.
 * @param check_interval The interval duration between checks.
 * @param serv The service name associated with this check.
 * @param cmd_name The command name for the check.
 * @param cmd_line The command line string for the check.
 * @param args RapidJSON value containing additional arguments for
 * configuration.
 * @param cnf Shared pointer to the engine-to-agent request configuration.
 * @param handler Completion handler to be called when the check is finished.
 * @param stat Shared pointer to the statistics collector for checks.
 *
 * @throws std::exception If argument parsing fails or required parameters are
 * missing.
 */
check_files::check_files(const std::shared_ptr<asio::io_context>& io_context,
                         const std::shared_ptr<spdlog::logger>& logger,
                         time_point first_start_expected,
                         duration check_interval,
                         const std::string& serv,
                         const std::string& cmd_name,
                         const std::string& cmd_line,
                         const rapidjson::Value& args,
                         const engine_to_agent_request_ptr& cnf,
                         check::completion_handler&& handler,
                         const checks_statistics::pointer& stat)
    : check(io_context,
            logger,
            first_start_expected,
            check_interval,
            serv,
            cmd_name,
            cmd_line,
            cnf,
            std::move(handler),
            stat) {
  com::centreon::common::rapidjson_helper arg(args);
  try {
    if (args.IsObject()) {
      _root_path = arg.get_string("path", "");
      _pattern = arg.get_string("pattern", "*.*");
      _max_depth = arg.get_int("max-depth", 0);
      _output_syntax = arg.get_string(
          "output-syntax",
          "${status}: ${problem_count}/${count} files (${problem_list})");
      _files_detail_syntax =
          arg.get_string("files-detail-syntax", "${filename}");
      _ok_syntax =
          arg.get_string("ok-syntax", "${status}: All ${count} files are ok");

      // filters
      _filter_files = arg.get_string("filter-files", "");
      _warning_status = arg.get_string("warning-status", "");
      _critical_status = arg.get_string("critical-status", "");

      // conditions to trigger warning and critical
      _warning_threshold_count = arg.get_int("warning-count", 1);
      _critical_threshold_count = arg.get_int("critical-count", 1);

      _verbose = arg.get_bool("verbose", false);
    }
  } catch (const std::exception& e) {
    SPDLOG_LOGGER_ERROR(_logger, "check_files failed to parse check params: {}",
                        e.what());
    throw;
  }

  if (_root_path.empty()) {
    SPDLOG_LOGGER_ERROR(_logger, "check_files: root path is empty");
    throw;
  }

  _build_checker();
  _calc_output_format();

  _filter = std::make_shared<check_files_detail::filter>(
      _root_path, _pattern, _max_depth, _line_count_needed, _file_filter.get());
}

/**
 * @brief Builds and configures the file checking filters and their associated
 * checkers.
 *
 * This method initializes the checker builder lambda, which sets up the
 * appropriate value extraction logic for different filter types and labels
 * (such as "size", "line_count", "creation", "access", "written", "filename",
 * and "version"). It then creates and configures the file, warning, and
 * critical status filters based on the provided filter expressions. Each filter
 * is assigned the checker builder to ensure correct evaluation of file metadata
 * against the filter criteria.
 *
 * @throws std::runtime_error If filter creation fails for any of the filter
 * types.
 * @throws exceptions::msg_fmt If an unknown filter label is encountered.
 */
void check_files::_build_checker() {
  if (_filter_files.find("line_count") != std::string::npos ||
      _warning_status.find("line_count") != std::string::npos ||
      _critical_status.find("line_count") != std::string::npos ||
      _files_detail_syntax.find("line_count") != std::string::npos) {
    _line_count_needed = true;
  }
  // create the checker for the filters
  _checker_builder = [](filter* f) {
    switch (f->get_type()) {
      case filter::filter_type::label_compare_to_value: {
        filters::label_compare_to_value* filt =
            static_cast<filters::label_compare_to_value*>(f);
        const std::string& label = filt->get_label();
        if (label == "size") {
          filt->calc_giga_mega_kilo();
        }
        if (label == "creation" || label == "access" || label == "written") {
          filt->calc_duration();
        }
        const double& value = filt->get_value();

        if (label == "line_count") {
          filt->set_checker_from_getter([](const testable& t) -> long {
            return (static_cast<const file_metadata&>(t)).number_of_lines;
          });
        } else if (label == "size") {
          filt->set_checker_from_getter([](const testable& t) -> long {
            return (static_cast<const file_metadata&>(t)).size;
          });
        } else if (label == "creation") {
          filt->set_checker_from_getter([](const testable& t) -> long {
            return std::chrono::duration_cast<std::chrono::seconds>(
                       std::chrono::file_clock::now() -
                       (static_cast<const file_metadata&>(t)).creation_time)
                .count();
          });
        } else if (label == "access") {
          filt->set_checker_from_getter([](const testable& t) -> long {
            return std::chrono::duration_cast<std::chrono::seconds>(
                       std::chrono::file_clock::now() -
                       (static_cast<const file_metadata&>(t)).last_access_time)
                .count();
          });
        } else if (label == "written") {
          filt->set_checker_from_getter([](const testable& t) -> long {
            return std::chrono::duration_cast<std::chrono::seconds>(
                       std::chrono::file_clock::now() -
                       (static_cast<const file_metadata&>(t)).last_write_time)
                .count();
          });
        } else {
          throw exceptions::msg_fmt("unknown filter label: {}", label);
        }
      } break;
      case filter::filter_type::label_compare_to_string: {
        filters::label_compare_to_string<char>* filt =
            static_cast<filters::label_compare_to_string<char>*>(f);
        std::string_view label = filt->get_label();
        if (label == "filename") {
          filt->set_checker_from_getter([](const testable& t) {
            return static_cast<const file_metadata&>(t).name;
          });
        } else if (label == "version") {
          filt->set_checker_from_getter([](const testable& t) {
            return static_cast<const file_metadata&>(t).version;
          });
        } else {
          throw exceptions::msg_fmt("unknown filter label: {}", label);
        }
      } break;
      case filter::filter_type::label_in: {
        filters::label_in<char>* filt =
            static_cast<filters::label_in<char>*>(f);
        std::string_view label = filt->get_label();
        if (label == "filename") {
          filt->set_checker_from_getter([](const testable& t) {
            return static_cast<const file_metadata&>(t).name;
          });
        } else if (label == "version") {
          filt->set_checker_from_getter([](const testable& t) {
            return static_cast<const file_metadata&>(t).version;
          });
        } else {
          throw exceptions::msg_fmt("unknown filter label: {}", label);
        }
      } break;
      default:
        break;
    }
  };

  // create filters
  if (!_filter_files.empty()) {
    _file_filter = std::make_unique<filters::filter_combinator>();

    if (!filter::create_filter(_filter_files, _logger, _file_filter.get(),
                               false, false)) {
      throw std::runtime_error("Failed to create filter for file filter");
    }
    _file_filter->apply_checker(_checker_builder);
    SPDLOG_LOGGER_DEBUG(_logger, "file filter created with filter: {}",
                        _filter_files);
  }

  if (!_warning_status.empty()) {
    _warning_rules_filter = std::make_unique<filters::filter_combinator>();

    if (!filter::create_filter(_warning_status, _logger,
                               _warning_rules_filter.get())) {
      throw std::runtime_error("Failed to create filter for warning status");
    }
    _warning_rules_filter->apply_checker(_checker_builder);
    SPDLOG_LOGGER_DEBUG(_logger, "Warning filter created with filter: {}",
                        _warning_status);
  }

  if (!_critical_status.empty()) {
    _critical_rules_filter = std::make_unique<filters::filter_combinator>();
    if (!filter::create_filter(_critical_status, _logger,
                               _critical_rules_filter.get())) {
      throw std::runtime_error("Failed to create filter for critical status");
    }
    _critical_rules_filter->apply_checker(_checker_builder);
    SPDLOG_LOGGER_DEBUG(_logger, "Critical filter created with filter: {}",
                        _critical_status);
  }
}

constexpr std::array<std::pair<std::string_view, std::string_view>, 20>
    _label_to_counter_detail{{{"${path}", "{0}"},
                              {"{path}", "{0}"},
                              {"${filename}", "{1}"},
                              {"{filename}", "{1}"},
                              {"${size}", "{2}"},
                              {"{size}", "{2}"},
                              {"${creation}", "{3}"},
                              {"{creation}", "{3}"},
                              {"${access}", "{4}"},
                              {"{access}", "{4}"},
                              {"${written}", "{5}"},
                              {"{written}", "{5}"},
                              {"${version}", "{6}"},
                              {"{version}", "{6}"},
                              {"${line_count}", "{7}"},
                              {"{line_count}", "{7}"},
                              {"${extension}", "{8}"},
                              {"{extension}", "{8}"}}};

constexpr std::array<std::pair<std::string_view, std::string_view>, 40>
    _label_to_output_index{{
        {"${status}", "{0}"},        {"${count}", "{1}"},
        {"${total}", "{2}"},         {"${list}", "{3}"},
        {"${warn_count}", "{4}"},    {"${warn-count}", "{4}"},
        {"${warn_list}", "{5}"},     {"${warn-list}", "{5}"},
        {"${crit_count}", "{6}"},    {"${crit-count}", "{6}"},
        {"${crit_list}", "{7}"},     {"${crit-list}", "{7}"},
        {"${problem_count}", "{8}"}, {"${problem-count}", "{8}"},
        {"${problem_list}", "{9}"},  {"${problem-list}", "{9}"},
        {"${ok_count}", "{10}"},     {"${ok-count}", "{10}"},
        {"${ok_list}", "{11}"},      {"${ok-list}", "{11}"},
        {"{status}", "{0}"},         {"{count}", "{1}"},
        {"{total}", "{2}"},          {"{list}", "{3}"},
        {"{warn_count}", "{4}"},     {"{warn-count}", "{4}"},
        {"{warn_list}", "{5}"},      {"{warn-list}", "{5}"},
        {"{crit_count}", "{6}"},     {"{crit-count}", "{6}"},
        {"{crit_list}", "{7}"},      {"{crit-list}", "{7}"},
        {"{problem_count}", "{8}"},  {"{problem-count}", "{8}"},
        {"{problem_list}", "{9}"},   {"{problem-list}", "{9}"},
        {"{ok_count}", "{10}"},      {"{ok-count}", "{10}"},
        {"{ok_list}", "{11}"},       {"{ok-list}", "{11}"},
    }};

/**
 * @brief Updates output format strings by replacing label placeholders with
 * their corresponding values.
 *
 * This method iterates over internal label-to-value mappings and applies
 * replacements to the output syntax strings used for reporting file check
 * results. Specifically:
 * - Replaces all occurrences of label placeholders in `_output_syntax` and
 * `_ok_syntax` using the `_label_to_output_index` mapping.
 * - Replaces all occurrences of label placeholders in `_files_detail_syntax`
 *   using the `_label_to_counter_detail` mapping.
 *
 * The replacements are performed in-place using `boost::replace_all`.
 * This ensures that the output strings reflect the current state of label
 * mappings.
 */
void check_files::_calc_output_format() {
  for (const auto& translate : _label_to_output_index) {
    boost::replace_all(_output_syntax, translate.first, translate.second);
  }
  for (const auto& translate : _label_to_counter_detail) {
    boost::replace_all(_files_detail_syntax, translate.first, translate.second);
  }
  for (const auto& translate : _label_to_output_index) {
    boost::replace_all(_ok_syntax, translate.first, translate.second);
  }
}

/**
 * @brief Formats the output string based on the file metadata and status.
 *
 * This method generates a formatted output string that summarizes the results
 * of the file check operation. It includes details such as the number of files
 * checked, their statuses (OK, WARNING, CRITICAL), and lists of files in each
 * category. The output is formatted according to the specified syntax and
 * includes detailed information about each file.
 *
 * @param map_data A map containing file paths and their associated metadata.
 * @param output A reference to a string where the formatted output will be
 * stored.
 * @param status The overall status of the file check operation (OK, WARNING,
 * CRITICAL).
 */
void check_files::_print_format(std::string& output, e_status status) {
  int total = _filter->get_files_metadata().size();
  int ok_count = static_cast<int>(_ok_list.size());
  int warn_count = static_cast<int>(_warning_list.size());
  int crit_count = static_cast<int>(_critical_list.size());
  int problem_count = warn_count + crit_count;
  int count = ok_count + problem_count;

  // format the detail output for a label,value
  auto format_detail = [this](const file_metadata& data) {
    std::string size = format_size(data.size);
    std::string creation_time = format_file_time(data.creation_time);
    std::string last_access_time = format_file_time(data.last_access_time);
    std::string last_write_time = format_file_time(data.last_write_time);

    try {
      return std::vformat(
          _files_detail_syntax,
          std::make_format_args(data.path, data.name, size, creation_time,
                                last_access_time, last_write_time, data.version,
                                data.number_of_lines, data.extension));
    } catch (const std::exception& e) {
      SPDLOG_LOGGER_ERROR(_logger,
                          "check files Failed to format file detail for {}",
                          _files_detail_syntax);
      // Fallback to a simple format if the detailed format fails
      return fmt::format("{} ({} bytes)", data.name, size);
    }
  };

  // format a map
  auto format_list =
      [this, &format_detail](const absl::btree_set<std::string>& data) {
        std::string result = "";
        const auto& files_metadata = _filter->get_files_metadata();
        for (const auto& path : data) {
          result += format_detail(*files_metadata.at(path)) + ",";
        }
        // remove the last comma
        if (!result.empty()) {
          result.pop_back();
        }
        return result;
      };

  // format the lists
  std::string _ok_list_str = format_list(_ok_list);
  std::string _warning_list_str = format_list(_warning_list);
  std::string _critical_list_str = format_list(_critical_list);
  std::string _problem_list_str = _critical_list_str;

  if (!_problem_list_str.empty() && !_warning_list_str.empty()) {
    _problem_list_str += ",";
  }
  _problem_list_str += _warning_list_str;

  std::string list_str = _problem_list_str;
  if (!list_str.empty() && !_ok_list_str.empty()) {
    list_str += ",";
  }
  list_str += _ok_list_str;

  std::string_view status_label;
  const std::string* chosen_syntax = &_output_syntax;

  switch (status) {
    case e_status::ok:
      status_label = "OK";
      chosen_syntax = &_ok_syntax;
      break;
    case e_status::warning:
      status_label = "WARNING";
      chosen_syntax = &_output_syntax;
      break;
    case e_status::critical:
      status_label = "CRITICAL";
      chosen_syntax = &_output_syntax;
      break;
    default:
      status_label = "UNKNOWN";
      chosen_syntax = &_output_syntax;
      break;
  }

  // format the output string
  try {
    output = std::vformat(
        *chosen_syntax,
        std::make_format_args(status_label, count, total, list_str, warn_count,
                              _warning_list_str, crit_count, _critical_list_str,
                              problem_count, _problem_list_str, ok_count,
                              _ok_list_str));
  } catch (const std::exception& e) {
    SPDLOG_LOGGER_ERROR(_logger,
                        "check files Failed to format output detail for {}",
                        *chosen_syntax);
    output = fmt::format("output syntax is incorrect : {}", e.what());
  }
}

/**
 * @brief Handles the completion of a file check operation.
 *
 * This method processes the results of a file check, determines the overall
 * status (OK, WARNING, or CRITICAL) based on configured rules and thresholds,
 * prepares performance data, and generates a detailed output message if verbose
 * mode is enabled. It then clears internal state and invokes the completion
 * callback.
 *
 * @param start_check_index Index identifying the start of the check operation.
 * @param result A map associating file paths with their corresponding metadata.
 *
 * The function performs the following steps:
 * - Checks if the result is empty and handles this case early.
 * - Applies critical and warning rules to each file, categorizing them
 * accordingly.
 * - Determines the final status based on the number of files in each category
 * and the configured thresholds.
 * - Prepares performance data for OK, WARNING, and CRITICAL file counts.
 * - Generates a detailed output string if verbose mode is enabled, including
 * file details.
 * - Clears internal lists and metadata.
 * - Calls the completion handler with the results.
 */
void check_files::_completion_handler(
    unsigned start_check_index,
    const absl::flat_hash_map<std::string, std::unique_ptr<file_metadata>>&
        result) {
  auto result_size = result.size();
  e_status ret = e_status::ok;
  std::string output;
  std::list<com::centreon::common::perfdata> perfs;

  if (result_size == 0) {
    SPDLOG_LOGGER_DEBUG(_logger,
                        "Check file: Empty or no match for this filter");
    output = "Empty or no match for this filter";
    on_completion(start_check_index, ret, perfs, {output});
    return;
  }

  // check warning and critical status
  for (const auto& [path, metadata] : result) {
    if (_critical_rules_filter && _critical_rules_filter->check(*metadata)) {
      _critical_list.insert(path);
    } else if (_warning_rules_filter &&
               _warning_rules_filter->check(*metadata)) {
      _warning_list.insert(path);
    } else {
      _ok_list.insert(path);
    }
  }

  // check the status
  if (_critical_list.size() != 0 &&
      _critical_list.size() >= _critical_threshold_count) {
    ret = e_status::critical;
  } else if (_warning_list.size() != 0 &&
             _warning_list.size() >= _warning_threshold_count) {
    ret = e_status::warning;
  } else {
    ret = e_status::ok;
  }

  // prepare performance data
  common::perfdata perf;
  perf.name("ok_count");
  perf.value(_ok_list.size());
  perfs.emplace_back(std::move(perf));

  perf.name("warning_count");
  perf.value(_warning_list.size());
  perfs.emplace_back(std::move(perf));

  perf.name("critical_count");
  perf.value(_critical_list.size());
  perfs.emplace_back(std::move(perf));

  if (_verbose) {
    std::string status_str;
    switch (ret) {
      case e_status::ok:
        status_str = "OK";
        break;
      case e_status::warning:
        status_str = "WARNING";
        break;
      case e_status::critical:
        status_str = "CRITICAL";
        break;
      default:
        status_str = "UNKNOWN";
        break;
    }
    output = fmt::format(
        "{}: Ok:{}|Nok:{}|total:{}  warning:{}|critical:{}\n", status_str,
        _ok_list.size(), _warning_list.size() + _critical_list.size(),
        result.size(), _warning_list.size(), _critical_list.size());
    for (const auto& [path, metadata_ptr] : result) {
      const file_metadata& metadata = *metadata_ptr;
      output += fmt::format(
          "{}: size: {}, created: {}, written: {}, accessed: {}", path,
          format_size(metadata.size), format_file_time(metadata.creation_time),
          format_file_time(metadata.last_write_time),
          format_file_time(metadata.last_access_time));

      if (_line_count_needed && metadata.number_of_lines != 0)
        output += fmt::format(",number of lines: {}", metadata.number_of_lines);
      if (metadata.version != "")
        output += fmt::format(",version: {}", metadata.version);
      output += '\n';
    }
  } else {
    _print_format(output, ret);
  }

  // clear the lists
  _filter->clear_files_metadata();
  _ok_list.clear();
  _warning_list.clear();
  _critical_list.clear();

  on_completion(start_check_index, ret, perfs, {output});
}

/**
 * @brief Starts the file check operation with a specified timeout.
 *
 * This method initiates the file checking process. If the check is not already
 * running, it creates and starts a worker thread responsible for performing the
 * file checks asynchronously. The method schedules an asynchronous file
 * retrieval operation, which will invoke the completion handler upon completion
 * or timeout.
 *
 * @param timeout The maximum duration to wait for the file check operation to
 * complete.
 *
 * @note If a file check is already running, this method will return immediately
 * without starting a new check.
 */
void check_files::start_check(const duration& timeout) {
  if (!check::_start_check(timeout)) {
    return;
  }
  if (!_worker_thread_files_check) {
    _worker_files_check =
        std::make_shared<check_files_detail::check_files_thread>(_io_context,
                                                                 _logger);
    _worker_thread_files_check =
        new std::thread([worker = _worker_files_check] { worker->run(); });
  }
  unsigned running_check_index = _get_running_check_index();
  _worker_files_check->async_get_files(
      _filter, std::chrono::system_clock::now() + timeout,
      [me = shared_from_this(), running_check_index](
          const absl::flat_hash_map<std::string,
                                    std::unique_ptr<file_metadata>>& result) {
        me->_completion_handler(running_check_index, result);
      });
}

/**
 * @brief Terminates and cleans up the worker thread responsible for file
 * checking.
 *
 * This method checks if the worker thread for file checking is active. If so,
 * it signals the associated worker to terminate, waits for the thread to finish
 * execution, and then deallocates the thread object. After cleanup, the thread
 * pointer is set to nullptr to prevent dangling references.
 */
void check_files::thread_kill() {
  if (_worker_thread_files_check) {
    _worker_files_check->kill();
    _worker_thread_files_check->join();
    delete _worker_thread_files_check;
    _worker_thread_files_check = nullptr;
  }
}

/**
 * @brief Display help information for the check_files command.
 *
 * @param help_stream The stream to write the help information to.
 * @return void
 */
void check_files::help(std::ostream& help_stream) {
  help_stream << R"(
Check_Files - File System Check for Centreon
--------------------------------------------
Checks files in a directory tree, applies filters, and evaluates file metadata (size, timestamps, version, line count, etc.) for monitoring and alerting.

JSON arguments
==============
{
  "path"                : string,                 # Root directory to search files in. (Required)
  "pattern"             : string,                 # Shell-style wildcards pattern to match filenames (default: "*.*")
  "max-depth"           : integer,                # Max recursion depth (0=top only, 1=+subdirs, -1=unlimited)
  "output-syntax"       : string,                 # Output format string for the overall check result.
                                                  # Placeholders: {status}, {count}, {total}, {list},
                                                  # {warn_count}, {crit_count}, {problem_count}, {ok_count}, etc.
                                                  # Default: "${status}: ${problem_count}/${count} files ${problem_list}"
  "files-detail-syntax" : string,                 # Format for each file detail inside {list}.
                                                  # Placeholders: {path}, {filename}, {size}, {creation},
                                                  # {access}, {write}, {version}, {line_count}, {extension}
                                                  # Default: "${filename}"
  "ok-syntax"           : string,                 # Output if all files are OK.
                                                  # Default: "${status}: All ${count} files are ok"
  "filter-files"        : string,                 # Filter expression to select files for the check.
                                                  # Example: "size > 1M && extension == '.dll'"
  "warning-status"      : string,                 # Filter expression: files matching are considered WARNING.
  "critical-status"     : string,                 # Filter expression: files matching are considered CRITICAL.
  "warning-count"       : integer (default: 1),   # Minimum WARNING files to set overall status to WARNING.
  "critical-count"      : integer (default: 1),   # Minimum CRITICAL files to set overall status to CRITICAL.
  "verbose"             : bool (default: false),  # Output detailed file info.
}

Placeholder reference
---------------------

output-syntax supports:
  {status} {count} {total} {list}
  {warn_count} {warn_list} {crit_count} {crit_list}
  {problem_count} {problem_list} {ok_count} {ok_list}

files-detail-syntax supports:
  {path} {filename} {size} {creation} {access}
  {write} {version} {line_count} {extension}

Default output uses human-readable file sizes (e.g., "1.23 Mo").

Filter expressions
------------------
Filter syntax is similar to C/SQL:
- Numeric operators: ==, !=, >, <, >=, <=
- Logical: && (AND), || (OR)
- String equality: ==, !=
- IN/NOT IN: version in ('1.0', '1.1')

Supported file metadata labels:
- size           (in bytes, supports units: K, Ko, M, Mo, G, Go)
- line_count     (line counting)
- creation       (file age in seconds since creation)
- access         (file age in seconds since last access)
- written        (file age in seconds since last modification)
- filename       (name of the file, string comparison)
- path           (full file path, string comparison)
- extension      (file extension, e.g. '.dll')
- version        (for .exe/.dll files, string comparison)

Examples of filters:
- "size > 1M"                   # Files larger than 1 MB
- "extension == '.dll'"          # DLL files only
- "line_count > 1000"            # Files with more than 1000 lines

Warning and Critical Status
---------------------------
Files matching the warning-status/critical-status filters are considered WARNING/CRITICAL.
You can combine with warning-count/critical-count to require multiple matches before changing the global state.

Examples:
- "size > 50M"                           # File larger than 50 MB is WARNING
- "extension == '.bak'"                   # Backup files trigger WARNING
- "size > 200M && extension == '.dll'"   # Large DLLs are CRITICAL

Example
-------

{
  "path"               : "C:\\Windows\\System32",
  "pattern"            : "*.dll",
  "max-depth"          : 1,
  "output-syntax"      : "{status}: {problem_count}/{count} DLLs have issues: {problem_list}",
  "files-detail-syntax": "{filename}: {size} {version}",
  "filter-files"       : "extension == '.dll'",
  "warning-status"     : "size > 10M",
  "critical-status"    : "size > 100M",
  "warning-count"      : 2,
  "critical-count"     : 1,
  "verbose"            : false
}

This will scan all DLLs in System32 (not recursing into subdirs), warn if >10M, critical if >100M, and only alert if there are 2+ warnings or 1+ criticals.

Note:
- If "line_count" is used in any filter or output, line count calculation will be enabled (may impact performance).
- Paths, patterns, and filters are case-insensitive on Windows.

)";
}
