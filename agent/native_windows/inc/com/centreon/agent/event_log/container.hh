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

#ifndef CENTREON_AGENT_CHECK_EVENT_LOG_CONTAINER_HH
#define CENTREON_AGENT_CHECK_EVENT_LOG_CONTAINER_HH

#include <chrono>
#include "event_log/data.hh"
#include "event_log/uniq.hh"

namespace com::centreon::agent::event_log {

/**
 * @class event_container
 * @brief Manages and processes event logs.
 *
 * The event_container class is responsible for managing event logs, including
 * filtering, decoding, and storing events. It provides mechanisms to start
 * event log subscriptions, handle events, and retrieve filtered event data.
 *
 * @details
 * - The class maintains separate containers for critical and warning events.
 * - It supports filtering events based on provided filters.
 * - It can decode message content if required.
 * - It provides methods to lock and unlock the internal mutex for thread-safe
 *   operations.
 *
 */
class event_container {
 public:
  using event_cont = std::multimap<std::chrono::file_clock::time_point, event>;

 private:
  duration _scan_range;

  const bool _need_to_decode_message_content;
  std::wstring _file;
  std::unique_ptr<event_filter> _primary_filter;
  std::unique_ptr<event_filter> _warning_filter;
  std::unique_ptr<event_filter> _critical_filter;
  std::unique_ptr<event_filter> _event_warning_filter;
  std::unique_ptr<event_filter> _event_critical_filter;

  event_cont _critical ABSL_GUARDED_BY(_events_m);
  event_cont _warning ABSL_GUARDED_BY(_events_m);
  std::multiset<std::chrono::file_clock::time_point> _ok_events
      ABSL_GUARDED_BY(_events_m);
  unsigned _insertion_cpt ABSL_GUARDED_BY(_events_m);
  absl::Mutex _events_m;

  EVT_HANDLE _render_context;
  EVT_HANDLE _subscription;

  using provider_metadata = absl::flat_hash_map<std::wstring, EVT_HANDLE>;
  provider_metadata _provider_metadata ABSL_GUARDED_BY(_events_m);

  void* _read_event_buffer ABSL_GUARDED_BY(_events_m);
  DWORD _buffer_size ABSL_GUARDED_BY(_events_m);

  LPWSTR _read_message_buffer ABSL_GUARDED_BY(_events_m);
  DWORD _message_buffer_size ABSL_GUARDED_BY(_events_m);  // size in wchar_t

  std::shared_ptr<spdlog::logger> _logger;

  static DWORD WINAPI _subscription_callback(EVT_SUBSCRIBE_NOTIFY_ACTION action,
                                             PVOID p_context,
                                             EVT_HANDLE h_event);

  LPWSTR _get_message_string(EVT_HANDLE h_metadata, EVT_HANDLE h_event);

  void _on_event(EVT_HANDLE event_handle);

 protected:
  void _on_event(const event_log::event_data& raw_event, EVT_HANDLE h_event)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(_events_m);

  virtual std::string _get_message_content(
      const event_log::event_data& raw_event,
      EVT_HANDLE h_event);

 public:
  event_container(const std::string_view& file,
                  const std::string_view& primary_filter,
                  const std::string_view& warning_filter,
                  const std::string_view& critical_filter,
                  duration scan_range,
                  bool need_to_decode_message_content,
                  const std::shared_ptr<spdlog::logger>& logger);
  void start();

  ~event_container();

  void lock() { _events_m.Lock(); }
  void unlock() { _events_m.Unlock(); }

  const event_cont& get_critical() const
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(_events_m) {
    return _critical;
  }
  const event_cont& get_warning() const
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(_events_m) {
    return _warning;
  }

  const std::wstring get_file() const { return _file; }

  void clean_perempted_events(bool check_filter_peremption)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(_events_m);

  unsigned get_nb_warning() const ABSL_EXCLUSIVE_LOCKS_REQUIRED(_events_m) {
    return _warning.size();
  }
  unsigned get_nb_critical() const ABSL_EXCLUSIVE_LOCKS_REQUIRED(_events_m) {
    return _critical.size();
  }
  size_t get_nb_ok() const ABSL_EXCLUSIVE_LOCKS_REQUIRED(_events_m) {
    return _ok_events.size();
  }
};

}  // namespace com::centreon::agent::event_log

#endif