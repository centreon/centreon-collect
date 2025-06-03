/**
 * Copyright 2024 Centreon (https://www.centreon.com/)
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * For more information : contact@centreon.com
 *
 */

#include <boost/exception/errinfo_file_name.hpp>
#include <boost/throw_exception.hpp>

#include "process_stat.hh"

using namespace com::centreon::common;

static auto read_file = [](const std::string& file_path) -> std::string {
  std::ifstream file(file_path);
  if (file.is_open()) {
    std::stringstream ss;
    ss << file.rdbuf();
    file.close();
    return ss.str();
  } else {
    BOOST_THROW_EXCEPTION(process_stat::exception()
                          << boost::errinfo_file_name(file_path));
  }
};

static auto extract_io_value = [](const std::string_view& label_value,
                                  const std::string& file_path) -> uint64_t {
  size_t value_index = label_value.find_first_of(" :");
  if (value_index == std::string_view::npos) {
    BOOST_THROW_EXCEPTION(process_stat::exception()
                          << process_stat::errinfo_bad_info_format(
                                 std::string(label_value.data()))
                          << boost::errinfo_file_name(file_path));
  }
  for (;
       value_index < label_value.length() &&
       (label_value.at(value_index) < '0' || label_value.at(value_index) > '9');
       ++value_index)
    ;
  uint64_t ret;
  if (value_index == label_value.length() ||
      !absl::SimpleAtoi(label_value.substr(value_index), &ret)) {
    BOOST_THROW_EXCEPTION(process_stat::exception()
                          << process_stat::errinfo_bad_info_format(
                                 std::string(label_value.data()))
                          << boost::errinfo_file_name(file_path));
  }
  return ret;
};

/**
 * @brief Construct a new process_stat object
 *
 * @throw process_stat::exception if an io error occurs
 * @param process_id pid
 */
process_stat::process_stat(pid_t process_id)
    : _pid(process_id),
      _num_threads(0),
      _query_read_bytes(0),
      _query_write_bytes(0),
      _real_read_bytes(0),
      _real_write_bytes(0) {
  std::string proc_path = fmt::format("/proc/{}/", process_id);
  std::string file_path(proc_path + "cmdline");
  _cmdline = read_file(file_path);
  // there are \0 between arguments
  for (auto& chr : _cmdline) {
    if (!chr) {
      chr = ' ';
    }
  }

  file_path = proc_path;
  file_path += "io";
  std::string file_content = read_file(file_path);
  auto io_lines = absl::StrSplit(file_content, '\n');
  for (const auto line : io_lines) {
    if (line.length() < 2) {
      continue;
    }
    char first_char = line.at(0), second_char = line.at(1);
    if (first_char == 'r') {
      if (second_char == 'c') {  // rchar
        _query_read_bytes = extract_io_value(line, file_path);
      } else if (second_char == 'e') {  // read_bytes
        _real_read_bytes = extract_io_value(line, file_path);
      }
    } else if (first_char == 'w') {
      if (second_char == 'c') {  // wchar
        _query_write_bytes = extract_io_value(line, file_path);
      } else if (second_char == 'r') {  // write_bytes
        _real_write_bytes = extract_io_value(line, file_path);
      }
    }
  }

  file_path = proc_path;
  file_path += "stat";
  file_content = read_file(file_path);
  auto stat_fields = absl::StrSplit(file_content, ' ');
  auto field_iter = stat_fields.begin();
  std::advance(field_iter, 13);
  uint64_t value = 0;
  absl::SimpleAtoi(*field_iter, &value);

  unsigned tick_per_second = sysconf(_SC_CLK_TCK);
  timespec ts_boot_time;
  clock_gettime(CLOCK_BOOTTIME, &ts_boot_time);
  std::chrono::system_clock::time_point boot_time =
      std::chrono::system_clock::now() -
      std::chrono::seconds(ts_boot_time.tv_sec);

  _user_time = std::chrono::milliseconds(value * 1000 / tick_per_second);
  std::advance(field_iter, 1);
  absl::SimpleAtoi(*field_iter, &value);
  _kernel_time = std::chrono::milliseconds(value * 1000 / tick_per_second);
  std::advance(field_iter, 5);
  absl::SimpleAtoi(*field_iter, &_num_threads);
  std::advance(field_iter, 2);
  absl::SimpleAtoi(*field_iter, &value);
  _starttime =
      boot_time + std::chrono::milliseconds(value * 1000 / tick_per_second);

  // statm file
  file_path.push_back('m');
  file_content = read_file(file_path);
  stat_fields = absl::StrSplit(file_content, ' ');
  unsigned page_size = getpagesize();
  field_iter = stat_fields.begin();
  absl::SimpleAtoi(*field_iter, &value);
  _vm_size = value * page_size;
  ++field_iter;
  absl::SimpleAtoi(*field_iter, &value);
  _res_size = value * page_size;
  ++field_iter;
  absl::SimpleAtoi(*field_iter, &value);
  _shared_size = value * page_size;
}
