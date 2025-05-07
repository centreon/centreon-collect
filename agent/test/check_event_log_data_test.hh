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

#ifndef CHECK_EVENT_LOG_DATA_TEST_HH
#define CHECK_EVENT_LOG_DATA_TEST_HH

#include "event_log/container.hh"
#include "event_log/data.hh"

namespace com::centreon::agent::event_log {
struct mock_event_data : public event_data {
  std::wstring provider;
  uint16_t event_id;
  uint8_t level;
  uint16_t task;
  int64_t keywords;
  uint64_t time_created;
  uint64_t record_id;
  std::wstring computer;
  std::wstring channel;

  mock_event_data() {}

  std::wstring_view get_provider() const override { return provider; }

  uint16_t get_event_id() const override { return event_id; }

  uint8_t get_level() const override { return level; }

  uint16_t get_task() const override { return task; }

  int64_t get_keywords() const override { return keywords; }

  uint64_t get_time_created() const override { return time_created; }

  uint64_t get_record_id() const override { return record_id; }

  std::wstring_view get_computer() const override { return computer; }

  std::wstring_view get_channel() const override { return channel; }

  void set_time_created(unsigned second_delta) {
    SYSTEMTIME st;
    GetSystemTime(&st);
    FILETIME ft;
    SystemTimeToFileTime(&st, &ft);
    ULARGE_INTEGER caster{.LowPart = ft.dwLowDateTime,
                          .HighPart = ft.dwHighDateTime};
    time_created = caster.QuadPart - second_delta * 10000000ULL;
  }
};

class test_event_container : public event_container {
 public:
  std::string message_content;

  test_event_container(const std::string_view& file,
                       const std::string_view& primary_filter,
                       const std::string_view& warning_filter,
                       const std::string_view& critical_filter,
                       duration scan_range,
                       bool need_to_decode_message_content,
                       const std::shared_ptr<spdlog::logger>& logger)
      : event_container(file,
                        primary_filter,
                        warning_filter,
                        critical_filter,
                        scan_range,
                        need_to_decode_message_content,
                        logger) {}

  void on_event(const event_log::event_data& raw_event, EVT_HANDLE h_event) {
    _on_event(raw_event, nullptr);
  }

  std::string _get_message_content(const event_log::event_data& raw_event,
                                   EVT_HANDLE h_event) override {
    return message_content;
  }
};

}  // namespace com::centreon::agent::event_log

#endif
