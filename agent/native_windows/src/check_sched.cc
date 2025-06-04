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

#include "check_sched.hh"

using namespace com::centreon::agent;
using Microsoft::WRL::ComPtr;

/**
 * @brief Convert a wide character string to a UTF-8 string.
 *
 * @param wstr The wide character string to convert.
 * @return The converted UTF-8 string.
 */
static std::string convert_wchar_tostring(const wchar_t* wstr) {
  int size = WideCharToMultiByte(CP_UTF8, 0, wstr, -1, NULL, 0, NULL, NULL);
  if (size <= 0)
    return "";
  std::string str(size - 1, 0);
  WideCharToMultiByte(CP_UTF8, 0, wstr, -1, &str[0], size, NULL, NULL);
  return str;
}

/**
 * @brief Convert a Date to a DateInfo structure containing a timestamp and a
 * formatted string.
 * @param date The DATE to convert.
 * @return A DateInfo structure containing the timestamp and formatted string.
 */
static DateInfo date_to_info(DATE date) {
  SYSTEMTIME st{};
  VariantTimeToSystemTime(date, &st);

  std::tm tm{};
  tm.tm_year = st.wYear - 1900;  // years since 1900
  tm.tm_mon = st.wMonth - 1;     // months since January [0–11]
  tm.tm_mday = st.wDay;          // day of month [1–31]
  tm.tm_hour = st.wHour;         // hours since midnight [0–23]
  tm.tm_min = st.wMinute;        // minutes after the hour [0–59]
  tm.tm_sec = st.wSecond;        // seconds after the minute [0–60]

  // Produce a formatted string "YYYY-MM-DD HH:MM:SS"
  std::ostringstream oss;
  oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
  std::string human_readable = oss.str();

  std::time_t t = _mkgmtime(&tm);
  std::chrono::system_clock::time_point tp =
      std::chrono::system_clock::from_time_t(t);
  return DateInfo{tp, std::move(human_readable)};
}

/**
 * @brief Construct a new check_sched::check_sched object
 *
 * @param io_context
 * @param logger
 * @param first_start_expected
 * @param check_interval
 * @param serv
 * @param cmd_name
 * @param cmd_line
 * @param args
 * @param cnf
 * @param handler
 */
check_sched::check_sched(const std::shared_ptr<asio::io_context>& io_context,
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
      // fomat the output
      _output_syntax =
          arg.get_string("output-syntax", "${status}: ${problem_list}");
      _task_detail_syntax = arg.get_string("task-detail-syntax",
                                           "${folder}${name}: ${exit_code}");
      _process_exclude_task(arg.get_string("exclude-tasks", ""));
      _ok_syntax = arg.get_string("ok-syntax", "OK: No problem tasks found");
      // filters
      _filter_tasks = arg.get_string("filter-tasks", "enabled == 1");
      _warning_status = arg.get_string("warning-status", "exit_code != 0");
      _critical_status = arg.get_string("critical-status", "exit_code < 0");

      // conditions to trigger warning and critical
      _warning_threshold_count = arg.get_int("warning-count", 1);
      _critical_threshold_count = arg.get_int("critical-count", 1);

      _verbose = arg.get_bool("verbose", false);
    }
  } catch (const std::exception& e) {
    SPDLOG_LOGGER_ERROR(_logger, "check_process, fail to parse arguments: {}",
                        e.what());
    throw;
  }
  // initialize the task filter
  _build_checker();
  // calculate the output format
  _calc_output_format();
}

/*
 * @brief start the check process.
 * @param timeout The timeout duration for the check.
 */
void check_sched::start_check(const duration& timeout) {
  if (!_start_check(timeout)) {
    return;
  }

  std::string output;
  std::list<common::perfdata> perfs;

  // Get all scheduled tasks
  get_all_scheduled_tasks();

  e_status status = compute(&output, &perfs);
  asio::post(
      *_io_context, [me = shared_from_this(), this, out = std::move(output),
                     status, performance = std::move(perfs)]() {
        on_completion(_get_running_check_index(), status, performance, {out});
      });
}

/**
 * @brief Compute the status of the scheduled tasks and prepare the output.
 *
 * @param output The output string to fill with the results.
 * @param perfs The performance data list to fill with task information.
 * @return The computed status of the check.
 */
e_status check_sched::compute(
    std::string* output,
    std::list<com::centreon::common::perfdata>* perfs) {
  e_status ret = e_status::ok;
  output->clear();

  // exclude tasks and filter tasks
  auto predicate = [&](auto const& task_data) {
    if (_exclude_tasks.contains(task_data.first)) {
      return true;  // Exclude task
    }
    if (!_task_filter->check(task_data.second))
      return true;  // filter this task if it does not match the filter

    return false;
  };

  absl::erase_if(_tasks, predicate);

  if (_tasks.empty()) {
    SPDLOG_LOGGER_INFO(
        _logger, "Check Task Scheduler: Empty or no match for this filter");
    *output = "Empty or no match for this filter";
    return e_status::ok;
  }

  // check warning and critical status
  for (const auto& [name, task_data] : _tasks) {
    if (perfs) {
      common::perfdata perf;
      perf.name(name);
      perf.value(task_data.info.exit_code);
      perf.unit("exit_code");
      perfs->emplace_back(std::move(perf));
    }
    if (_critical_rules_filter && _critical_rules_filter->check(task_data)) {
      _critical_list.insert(name);
    } else if (_warning_rules_filter &&
               _warning_rules_filter->check(task_data)) {
      _warning_list.insert(name);
    } else {
      _ok_list.insert(name);
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
  if (perfs) {
    common::perfdata perf;
    perf.name("ok_count");
    perf.value(_ok_list.size());
    perfs->emplace_back(std::move(perf));

    perf.name("warning_count");
    perf.value(_warning_list.size());
    perfs->emplace_back(std::move(perf));

    perf.name("critical_count");
    perf.value(_critical_list.size());
    perfs->emplace_back(std::move(perf));
  }
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
    *output = fmt::format(
        "{}: Ok:{}|Nok:{}|total:{}  warning:{}|critical:{}\n", status_str,
        _ok_list.size(), _warning_list.size() + _critical_list.size(),
        _tasks.size(), _warning_list.size(), _critical_list.size());
    for (const auto& [name, task_data] : _tasks) {
      *output += task_data.info.folder + task_data.info.name + ": " +
                 "last run: " + task_data.info.last_run.formatted +
                 " next run " + task_data.info.next_run.formatted +
                 " (exit code: " +
                 fmt::format("{0:#x}",
                             static_cast<uint32_t>(task_data.info.exit_code)) +
                 ")\n";
    }
  } else {
    _print_format(output, ret);
  }

  // clear the lists
  _ok_list.clear();
  _warning_list.clear();
  _critical_list.clear();
  _tasks.clear();

  return ret;
}

/**
 * @brief Get all scheduled tasks from the Task Scheduler.
 * This function initializes COM, connects to the Task Scheduler service,
 * retrieves all registered tasks, and enumerates them.
 */
void check_sched::get_all_scheduled_tasks() {
  /*
    1- Initialize COM and set general COM security.
    2- Create an instance of the Task Service.
    3- Connect to the task service.
  */

  HRESULT hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
  if (FAILED(hr)) {
    SPDLOG_LOGGER_ERROR(
        _logger,
        "Check Task Scheduler: CoInitializeEx failed with error code: {}", hr);
    return;
  }

  //  Set general COM security levels.
  hr = CoInitializeSecurity(NULL, -1, NULL, NULL, RPC_C_AUTHN_LEVEL_PKT_PRIVACY,
                            RPC_C_IMP_LEVEL_IMPERSONATE, NULL, 0, NULL);

  if (FAILED(hr)) {
    SPDLOG_LOGGER_ERROR(_logger,
                        "Check Task Scheduler: CoInitializeSecurity failed "
                        "with error code: {}",
                        hr);
    return;
  }
  //  ------------------------------------------------------
  {
    //  Create an instance of the Task Service.
    ComPtr<ITaskService> service_ptr;
    hr = CoCreateInstance(CLSID_TaskScheduler, NULL, CLSCTX_INPROC_SERVER,
                          IID_ITaskService, (void**)&service_ptr);
    if (FAILED(hr)) {
      SPDLOG_LOGGER_ERROR(
          _logger,
          "Check Task Scheduler: Failed to CoCreate an instance "
          "of the TaskService "
          "class with error code: {}",
          hr);
      CoUninitialize();
      return;
    }

    //  Connect to the task service.
    hr = service_ptr->Connect(_variant_t(), _variant_t(), _variant_t(),
                              _variant_t());
    if (FAILED(hr)) {
      SPDLOG_LOGGER_ERROR(
          _logger, "Check Task Scheduler: ITaskService connect failed: {:#X}",
          hr);
      CoUninitialize();
      return;
    }

    // Get the pointer to the root task folder.
    ComPtr<ITaskFolder> root_folder_ptr;
    hr = service_ptr->GetFolder(_bstr_t(L"\\"), &root_folder_ptr);

    if (FAILED(hr)) {
      SPDLOG_LOGGER_ERROR(
          _logger,
          "Check Task Scheduler: Cannot get Root Folder pointer: "
          "{:#X}",
          hr);

      CoUninitialize();
      return;
    }

    // Get the registered tasks in the folder.
    _enumerate_tasks(root_folder_ptr);
  }
  // uninitialize COM
  CoUninitialize();
  SPDLOG_LOGGER_INFO(_logger, "Check Task Scheduler: task found = {}",
                     _task_count);
}

/**
 * @brief Enumerate tasks in the specified folder and its subfolders.
 *
 * @param root_folder_ptr The pointer to the root task folder.
 */
void check_sched::_enumerate_tasks(ComPtr<ITaskFolder> root_folder_ptr) {
  ComPtr<IRegisteredTaskCollection> task_collection_ptr;
  if (FAILED(
          root_folder_ptr->GetTasks(TASK_ENUM_HIDDEN, &task_collection_ptr))) {
    SPDLOG_LOGGER_ERROR(_logger, "Cannot get the registered tasks.");
    return;
  }

  LONG num_task = 0;
  if (FAILED(task_collection_ptr->get_Count(&num_task))) {
    return;
  }

  if (num_task != 0) {
    TASK_STATE task_state;
    for (LONG i = 0; i < num_task; i++) {
      ComPtr<IRegisteredTask> registered_task_ptr;
      if (FAILED(task_collection_ptr->get_Item(_variant_t(i + 1),
                                               &registered_task_ptr))) {
        SPDLOG_LOGGER_ERROR(
            _logger, "Cannot get the registered task item at index={}", i + 1);
        continue;
      }
      _get_task_info(registered_task_ptr);
    }
  }

  // get the subfolders
  ComPtr<ITaskFolderCollection> sub_folders_ptr;
  if (FAILED(root_folder_ptr->GetFolders(TASK_ENUM_HIDDEN, &sub_folders_ptr))) {
    SPDLOG_LOGGER_ERROR(_logger, "Cannot get the subfolders.");
    return;
  }
  LONG num_folders = 0;
  if (FAILED(sub_folders_ptr->get_Count(&num_folders))) {
    SPDLOG_LOGGER_ERROR(_logger, "Cannot get the number of subfolders.");
    return;
  }

  for (LONG i = 0; i < num_folders; ++i) {
    ComPtr<ITaskFolder> folder_ptr;
    if (FAILED(sub_folders_ptr->get_Item(_variant_t(i + 1), &folder_ptr))) {
      SPDLOG_LOGGER_ERROR(_logger, "Cannot get the subfolder item at index={}",
                          i + 1);
      continue;
    }
    _enumerate_tasks(folder_ptr);
  }
}

/**
 * @brief Get information about a specific task and store it in the _tasks map.
 *
 * @param task The IRegisteredTask pointer to retrieve information from.
 */
void check_sched::_get_task_info(ComPtr<IRegisteredTask> task) {
  /*
    get all task information:
    - folder
    - name
    - state
    - enabled
    - last run time
    - next run time
    - last result
    - author
    - description
  */
  tasksched_data data;

  BSTR bstr_folder = NULL;
  if (SUCCEEDED(task->get_Path(&bstr_folder))) {
    // delete the name of the task from the path but keep the root folder
    // e.g. "\Microsoft\Windows\TaskName" -> "\Microsoft\Windows"
    std::string folder_path = convert_wchar_tostring(bstr_folder);
    size_t last_slash = folder_path.find_last_of('\\');
    if (last_slash != std::string::npos && last_slash != 0) {
      // extract the name of the task
      data.info.name = folder_path.substr(last_slash + 1);
      data.info.folder = folder_path.substr(0, last_slash + 1);
    } else {
      data.info.name = folder_path.substr(last_slash + 1);
      data.info.folder = "\\";
    }
    SysFreeString(bstr_folder);
  } else {
    SPDLOG_LOGGER_ERROR(_logger, "Failed to get task folder.");
    return;  // If we can't get the folder, we can't proceed with this task.
  }

  TASK_STATE state;
  if (SUCCEEDED(task->get_State(&state))) {
    data.info.state = state;
  } else {
    SPDLOG_LOGGER_ERROR(_logger, "Failed to get task state.");
  }

  VARIANT_BOOL is_enabled;
  if (SUCCEEDED(task->get_Enabled(&is_enabled))) {
    data.info.enabled = (is_enabled == VARIANT_TRUE);
  } else {
    SPDLOG_LOGGER_ERROR(_logger, "Failed to get task enabled state.");
  }

  DATE last_run_time;
  if (SUCCEEDED(task->get_LastRunTime(&last_run_time))) {
    data.info.last_run = date_to_info(last_run_time);
    data.info.duration_last_run =
        std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now() - data.info.last_run.timestamp)
            .count();
  } else {
    SPDLOG_LOGGER_ERROR(_logger, "Failed to get task last run time.");
  }

  DATE next_run_time;
  if (SUCCEEDED(task->get_NextRunTime(&next_run_time))) {
    data.info.next_run = date_to_info(next_run_time);
  } else {
    SPDLOG_LOGGER_ERROR(_logger, "Failed to get task next run time.");
  }

  LONG last_task_result = 0;
  if (SUCCEEDED(task->get_LastTaskResult(&last_task_result))) {
    data.info.exit_code = last_task_result;
  } else {
    SPDLOG_LOGGER_ERROR(_logger, "Failed to get task last result.");
  }

  LONG number_missed_runs = 0;
  if (SUCCEEDED(task->get_NumberOfMissedRuns(&number_missed_runs))) {
    data.info.number_missed_runs = number_missed_runs;
  } else {
    SPDLOG_LOGGER_ERROR(_logger, "Failed to get number of missed runs.");
  }

  ComPtr<ITaskDefinition> task_definition_ptr;
  if (SUCCEEDED(task->get_Definition(&task_definition_ptr))) {
    ComPtr<IRegistrationInfo> reg_info_ptr;
    if (SUCCEEDED(task_definition_ptr->get_RegistrationInfo(&reg_info_ptr))) {
      BSTR author = NULL;
      if (SUCCEEDED(reg_info_ptr->get_Author(&author))) {
        data.info.author = convert_wchar_tostring(author);
        SysFreeString(author);
      } else {
        SPDLOG_LOGGER_ERROR(_logger, "Failed to get task author.");
      }

      BSTR description = NULL;
      if (SUCCEEDED(reg_info_ptr->get_Description(&description))) {
        data.info.description = convert_wchar_tostring(description);
        SysFreeString(description);
      } else {
        SPDLOG_LOGGER_ERROR(_logger, "Failed to get task description.");
      }

    } else {
      SPDLOG_LOGGER_ERROR(_logger, "Failed to get task registration info.");
    }
  } else {
    SPDLOG_LOGGER_ERROR(_logger, "Failed to get task definition.");
  }
  std::string lower_label(data.info.name);
  std::ranges::transform(lower_label, lower_label.begin(), ::tolower);
  _tasks.emplace(std::move(lower_label), std::move(data));
  _task_count++;
}

/**
 * @brief Build the checker and the filter for the warning and critical
 * thresholds
 *
 * @return void
 */
void check_sched::_build_checker() {
  // process the filter syntax to convert hex to long
  auto transform_hex_tolong = [](std::string& filter) {
    auto search_pos = 0;
    while (true) {
      auto pos = filter.find("0x", search_pos);
      if (pos != std::string::npos) {
        // extract the substring after "0x" until the next space or end of
        // string
        std::string extracted =
            filter.substr(pos + 2, filter.find(" ", pos + 2) - (pos + 2));
        // convert to long
        long value;
        try {
          value = std::stoul(extracted, nullptr, 16);
        } catch (const std::exception& e) {
          search_pos = pos + extracted.length() + 2;
          continue;  // Skip if conversion fails
        }
        std::string extracted_value = std::to_string(value);
        filter.replace(pos, extracted.length() + 2, extracted_value);
        search_pos = pos + extracted_value.length();  // Update search position
      } else {
        break;
      }
    }
  };

  // create the checker for the filters
  _checker_builder = [](filter* f) {
    switch (f->get_type()) {
      case filter::filter_type::label_compare_to_value: {
        filters::label_compare_to_value* filt =
            static_cast<filters::label_compare_to_value*>(f);
        const std::string& label = filt->get_label();
        if (label == "last_run" || label == "next_run") {
          filt->calc_duration();
        }
        const double& value = filt->get_value();
        filt->set_checker_from_getter(
            [label, value](const testable& t) -> long {
              const auto& data = static_cast<const tasksched_data&>(t);
              if (label == "enabled") {
                return (data.info.enabled ? 1l : 0l);
              } else if (label == "exit_code") {
                return data.info.exit_code;
              } else if (label == "missed_runs") {
                return data.info.number_missed_runs;
              } else if (label == "last_run") {
                return data.info.duration_last_run;
              }
              return 0l;
            });
      } break;
      case filter::filter_type::label_compare_to_string: {
        filters::label_compare_to_string<char>* filt =
            static_cast<filters::label_compare_to_string<char>*>(f);
        std::string_view label = filt->get_label();
        if (label == "name") {
          filt->set_checker_from_getter([](const testable& t) {
            return static_cast<const tasksched_data&>(t).info.name;
          });
        } else if (label == "folder") {
          filt->set_checker_from_getter([](const testable& t) {
            return static_cast<const tasksched_data&>(t).info.folder;
          });
        } else if (label == "author") {
          filt->set_checker_from_getter([](const testable& t) {
            return static_cast<const tasksched_data&>(t).info.author;
          });
        } else if (label == "state") {
          filt->set_checker_from_getter([](const testable& t) {
            const auto& data = static_cast<const tasksched_data&>(t);
            switch (data.info.state) {
              case TASK_STATE::TASK_STATE_DISABLED:
                return std::string("disabled");
              case TASK_STATE::TASK_STATE_QUEUED:
                return std::string("queued");
              case TASK_STATE::TASK_STATE_READY:
                return std::string("ready");
              case TASK_STATE::TASK_STATE_RUNNING:
                return std::string("running");
              default:
                return std::string("unknown");
            }
          });
        }
      } break;
      default:
        break;
    }
  };

  if (!_filter_tasks.empty()) {
    // transform the hex to long
    transform_hex_tolong(_filter_tasks);
    _task_filter = std::make_unique<filters::filter_combinator>();

    if (!filter::create_filter(_filter_tasks, _logger, _task_filter.get())) {
      throw std::runtime_error("Failed to create filter for task filter");
    }
    _task_filter->apply_checker(_checker_builder);
    SPDLOG_LOGGER_INFO(_logger, "Task filter created with filter: {}",
                       _filter_tasks);
  }

  // create the filter for the warning
  if (!_warning_status.empty()) {
    // transform the hex to long
    transform_hex_tolong(_warning_status);
    _warning_rules_filter = std::make_unique<filters::filter_combinator>();

    if (!filter::create_filter(_warning_status, _logger,
                               _warning_rules_filter.get())) {
      throw std::runtime_error("Failed to create filter for warning status");
    }
    _warning_rules_filter->apply_checker(_checker_builder);
    SPDLOG_LOGGER_INFO(_logger, "Warning filter created with filter: {}",
                       _warning_status);
  }

  // create the filter for the critical
  if (!_critical_status.empty()) {
    // transform the hex to long
    transform_hex_tolong(_critical_status);
    _critical_rules_filter = std::make_unique<filters::filter_combinator>();
    if (!filter::create_filter(_critical_status, _logger,
                               _critical_rules_filter.get())) {
      throw std::runtime_error("Failed to create filter for critical status");
    }
    _critical_rules_filter->apply_checker(_checker_builder);
    SPDLOG_LOGGER_INFO(_logger, "Critical filter created with filter: {}",
                       _critical_status);
  }
}

/**
 * @brief Process the exclude tasks parameter and populate the _exclude_tasks
 * set.
 *
 * @param param The parameter string containing task labels to exclude.
 */
void check_sched::_process_exclude_task(const std::string_view& param) {
  if (param.empty()) {
    return;
  }
  for (std::string_view label : absl::StrSplit(param, ',')) {
    // make sure to transform the label to lower case
    std::string lower_label(label);
    std::ranges::transform(lower_label, lower_label.begin(), ::tolower);
    _exclude_tasks.insert(std::move(lower_label));
  }
}

constexpr std::array<std::pair<std::string_view, std::string_view>, 20>
    _label_to_counter_detail{
        {{"${folder}", "{0}"},       {"{folder}", "{0}"},
         {"${name}", "{1}"},         {"{name}", "{1}"},
         {"${exit_code}", "{2:#x}"}, {"{exit_code}", "{2:#x}"},
         {"${next_run}", "{3}"},     {"{next_run}", "{3}"},
         {"${last_run}", "{4}"},     {"{last_run}", "{4}"},
         {"${missed_runs}", "{5}"},  {"{missed_runs}", "{5}"},
         {"${state}", "{6}"},        {"{state}", "{6}"},
         {"${author}", "{7}"},       {"{author}", "{7}"},
         {"${description}", "{8}"},  {"{description}", "{8}"},
         {"${enabled}", "{9}"},      {"{enabled}", "{9}"}}};

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
 * @brief Calculate the output format for the check sched.
 *
 * @param param The output format string.
 * @return void
 */
void check_sched::_calc_output_format() {
  //
  for (const auto& translate : _label_to_output_index) {
    boost::replace_all(_output_syntax, translate.first, translate.second);
  }
  for (const auto& translate : _label_to_counter_detail) {
    boost::replace_all(_task_detail_syntax, translate.first, translate.second);
  }
  for (const auto& translate : _label_to_output_index) {
    boost::replace_all(_ok_syntax, translate.first, translate.second);
  }
}

/*
 * @brief Print the formatted output based on the current status and task
 * lists.
 *
 * @param output The output string to fill with the formatted results.
 * @param status The current status of the check.
 */
void check_sched::_print_format(std::string* output, e_status status) {
  int total = static_cast<int>(_tasks.size());
  int ok_count = static_cast<int>(_ok_list.size());
  int warn_count = static_cast<int>(_warning_list.size());
  int crit_count = static_cast<int>(_critical_list.size());
  int problem_count = warn_count + crit_count;
  int count = ok_count + problem_count;

  // Helper: convert TASK_STATE → string_view
  auto state_to_string = [](TASK_STATE s) -> std::string {
    switch (s) {
      case TASK_STATE::TASK_STATE_DISABLED:
        return "disabled";
      case TASK_STATE::TASK_STATE_QUEUED:
        return "queued";
      case TASK_STATE::TASK_STATE_READY:
        return "ready";
      case TASK_STATE::TASK_STATE_RUNNING:
        return "running";
      default:
        return "unknown";
    }
  };

  // format the detail output for a label,value
  auto format_detail = [this, &state_to_string](task_data& data) {
    std::string enabled_str = (data.enabled ? "True" : "False");
    std::string state_str_view = state_to_string(data.state);
    unsigned exit_code = static_cast<uint32_t>(data.exit_code);

    return std::vformat(
        _task_detail_syntax,
        std::make_format_args(data.folder, data.name, exit_code,
                              data.next_run.formatted, data.last_run.formatted,
                              data.number_missed_runs, state_str_view,
                              data.author, data.description, enabled_str));
  };

  // format a map
  auto format_list =
      [this, &format_detail](const absl::btree_set<std::string>& data_map) {
        std::string result = "";
        for (const auto& name : data_map) {
          result += format_detail(_tasks[name].info) + ",";
        }
        // remove the last comma
        if (!result.empty()) {
          result.pop_back();
        }
        return result;
      };

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

  std::string status_label;
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
  *output = std::vformat(
      *chosen_syntax,
      std::make_format_args(status_label, count, total, list_str, warn_count,
                            _warning_list_str, crit_count, _critical_list_str,
                            problem_count, _problem_list_str, ok_count,
                            _ok_list_str));
}

/**
 * @brief Display help information for the check_tasksched command.
 *
 * @param help_stream The stream to write the help information to.
 * @return void
 */
static void help(std::ostream& help_stream) {
  help_stream << R"(
Check_TaskSched - Windows Task Scheduler check for Centreon
-----------------------------------------------------------
JSON arguments
==============
  {
    "output-syntax"      : string,                 # Format the output.
                                                     Place-holders: {status},
                                                     {count}, {total},
                                                     {list}, {warn_count},
                                                     {crit_count}, etc.
                                                     Default: "{status}: {problem_list}"
    "task-detail-syntax" : string,                 # Format for each task detail
                                                     inside {list}.
                                                     Place-holders: {folder},
                                                     {name}, {exit_code}, etc.
                                                     Default: "${folder}: ${exit_code}"
    "exclude-tasks"      : string,                 # Comma-separated list of
                                                     task names to exclude.
    "filter-tasks"       : string,                 # Filter expression that
                                                     determines which tasks to check.
                                                     Default: "enabled == 1"
    "warning-status"     : string,                 # Filter expression that
                                                     marks a task WARNING.
                                                     Default: "exit_code != 0"
    "critical-status"    : string,                 # Filter expression that
                                                     marks a task CRITICAL.
                                                     Default: "exit_code < 0"
    "warning-count"      : integer (default 1),    # Minimum WARNING tasks
                                                     before overall status is
                                                     WARNING.
    "critical-count"     : integer (default 1),    # Minimum CRITICAL tasks
                                                     before overall status is
                                                     CRITICAL.
    "verbose"            : bool (default false),    # Add verbose output including
                                                     detailed task information.
  }

Place-holder reference
----------------------

output-syntax supports:  
{status} {count} {total} {list}  
{warn_count} {warn_list} {crit_count} {crit_list}  
{problem_count} {problem_list} {ok_count} {ok_list}

task-detail-syntax supports:  
{folder} {name} {exit_code} {next_run} {last_run}
{missed_runs} {state} {author} {description} {enabled}

Note: {exit_code} is printed in hexadecimal format.

Filter expressions (filter-tasks)
-----------------
You can filter tasks using expressions like:
- "enabled == 1" - Only enabled tasks
- "exit_code != 0" - Tasks with non-zero exit code
- "missed_runs > 0" - Tasks with missed runs
- "state == 'running'" - Tasks in running state
- "name == 'TaskName'" - Specific task by name
- "folder == '\\Folder\\To\\Task'" - Specific task by path

Warning and Critical Status
--------------------------
You can define conditions for warning and critical statuses using expressions like:
- "exit_code != 0" - Task with non-zero exit code is WARNING
- "exit_code < 0 || missed_runs > 2" - Task with negative exit code or more than 2 missed runs is CRITICAL


Example
-------

```json
{
  "output-syntax"      : "{status}: {problem_count}/{total} tasks have issues",
  "task-detail-syntax" : "{name}: exit code={exit_code}",
  "exclude-tasks"      : "Task1,Task2",
  "filter-tasks"       : "enabled == 1",
  "warning-status"     : "exit_code != 0",
  "critical-status"    : "exit_code < 0 || missed_runs > 2",
  "warning-count"      : 1,
  "critical-count"     : 1,
  "verbose"            : false
})";
}
