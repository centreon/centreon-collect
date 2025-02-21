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

#include "windows_util.hh"

#include "check_event_log_container.hh"

using namespace com::centreon::agent;
using namespace com::centreon::agent::check_event_log_detail;

event_container::event_container(const std::string_view& file,
                                 const std::string_view& unique_str,
                                 const std::string_view& primary_filter,
                                 const std::string_view& warning_filter,
                                 const std::string_view& critical_filter,
                                 duration scan_range,
                                 const std::shared_ptr<spdlog::logger>& logger)
    : _scan_range(scan_range),
      _file(file.begin(), file.end()),
      _event_compare(unique_str, logger),
      _critical(0, _event_compare, _event_compare),
      _insertion_cpt(0),
      _nb_warning(0),
      _nb_critical(0),
      _render_context(nullptr),
      _subscription(nullptr),
      _read_event_buffer(malloc(4096)),
      _buffer_size(4096),
      _read_message_buffer(static_cast<LPWSTR>(malloc(4096 * sizeof(wchar_t)))),
      _message_buffer_size(4095),
      _logger(logger) {
  if (!primary_filter.empty()) {
    try {
      _primary_filter = std::make_unique<event_filter>(primary_filter, logger);
    } catch (const std::exception& e) {
      SPDLOG_LOGGER_ERROR(logger, "fail to parse event filter: {}", e.what());
      throw;
    }
  }

  if (!warning_filter.empty()) {
    try {
      _warning_filter = std::make_unique<event_filter>(warning_filter, logger);
    } catch (const std::exception& e) {
      SPDLOG_LOGGER_ERROR(logger, "fail to parse warning filter: {}", e.what());
      throw;
    }
  }

  if (!critical_filter.empty()) {
    try {
      _critical_filter =
          std::make_unique<event_filter>(critical_filter, logger);
    } catch (const std::exception& e) {
      SPDLOG_LOGGER_ERROR(logger, "fail to parse critical filter: {}",
                          e.what());
      throw;
    }
  }

  _render_context = EvtCreateRenderContext(0, nullptr, EvtRenderContextSystem);
  if (!_render_context) {
    throw exceptions::msg_fmt("fail to create render context: {}",
                              get_last_error_as_string());
  }
}

event_container::~event_container() {
  if (_subscription != nullptr) {
    EvtClose(_subscription);
  }
  if (_render_context != nullptr) {
    EvtClose(_render_context);
  }
  free(_read_event_buffer);
  free(_read_message_buffer);

  for (auto& to_close : _provider_metadata) {
    EvtClose(to_close.second);
  }
}

void event_container::start() {
  std::wstring query;
  query = std::format(
      L"Event/System[TimeCreated[timediff(@SystemTime) <= {}]]",
      std::chrono::duration_cast<std::chrono::milliseconds>(_scan_range)
          .count());

  _subscription = EvtSubscribe(nullptr, nullptr, _file.c_str(), query.c_str(),
                               nullptr, this, _subscription_callback,
                               EvtSubscribeStartAtOldestRecord);
  if (_subscription == nullptr) {
    throw exceptions::msg_fmt("Failed to subscribe to event log: {}",
                              get_last_error_as_string());
  }
}

DWORD WINAPI
event_container::_subscription_callback(EVT_SUBSCRIBE_NOTIFY_ACTION action,
                                        PVOID p_context,
                                        EVT_HANDLE h_event) {
  event_container* me = reinterpret_cast<event_container*>(p_context);
  if (action == EvtSubscribeActionError) {
    SPDLOG_LOGGER_ERROR(me->_logger, "subscription_callback error");
    return ERROR_SUCCESS;
  }
  me->_on_event(h_event);
  return ERROR_SUCCESS;
}

LPWSTR event_container::_get_message_string(EVT_HANDLE h_metadata,
                                            EVT_HANDLE h_event) {
  DWORD buffer_used = 0;

  if (EvtFormatMessage(h_metadata, h_event, 0, 0, nullptr,
                       EvtFormatMessageEvent, _message_buffer_size,
                       _read_message_buffer, &buffer_used)) {
    _read_message_buffer[_message_buffer_size] = L'\0';
    return _read_message_buffer;
  }
  if (GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
    _read_message_buffer = static_cast<LPWSTR>(
        malloc((buffer_used + 1) * sizeof(wchar_t)));  //+1 for \0
    _message_buffer_size = buffer_used;
  }
  if (EvtFormatMessage(h_metadata, h_event, 0, 0, nullptr,
                       EvtFormatMessageEvent, _message_buffer_size,
                       _read_message_buffer, &buffer_used)) {
    _read_message_buffer[_message_buffer_size] = L'\0';
    return _read_message_buffer;
  }
  SPDLOG_LOGGER_ERROR(_logger, "fail to get message");
  return nullptr;
}

void event_container::_on_event(EVT_HANDLE h_event) {
  try {
    absl::MutexLock l(&_events_m);
    event_data raw_event(_render_context, h_event, &_read_event_buffer,
                         &_buffer_size);

    if (!_primary_filter->allow(raw_event)) {
      return;
    }

    e_status event_status = e_status::ok;
    if (_critical_filter->allow(raw_event)) {
      event_status = e_status::critical;
    } else if (_warning_filter->allow(raw_event)) {
      event_status = e_status::warning;
    } else {
      return;
    }

    std::string message;
    if (_need_to_decode_message_content) {
      auto yet_open = _provider_metadata.find(raw_event.get_provider());
      if (yet_open == _provider_metadata.end()) {
        EVT_HANDLE new_publisher = EvtOpenPublisherMetadata(
            NULL, raw_event.get_provider().data(), NULL, 0, 0);
        if (!new_publisher) {
          SPDLOG_LOGGER_ERROR(_logger,
                              "fail to get publisher metadata for provider: {}",
                              lpwcstr_to_acp(raw_event.get_provider().data()));
        } else {
          yet_open = _provider_metadata
                         .emplace(raw_event.get_provider(), new_publisher)
                         .first;
        }
      }
      if (yet_open != _provider_metadata.end()) {
        LPWSTR mess = _get_message_string(yet_open->second, h_event);
        if (mess) {
          message = lpwcstr_to_acp(mess);
        }
      }
    }

    boost::replace_all(message, "\r\n", " ");

    event to_ins(raw_event, event_status, std::move(message));

    if (event_status == e_status::warning) {
      ++_nb_warning;
      auto res_insert = _warning.emplace(std::move(to_ins), to_ins.time());
      if (!res_insert.second && res_insert.first->second < to_ins.time()) {
        res_insert.first->second = to_ins.time();
      }
    } else {
      ++_nb_critical;
      auto res_insert = _critical.emplace(std::move(to_ins), to_ins.time());
      if (!res_insert.second && res_insert.first->second < to_ins.time()) {
        res_insert.first->second = to_ins.time();
      }
    }

    // every 10 events we clean oldest events
    if (!((++_insertion_cpt) % 10)) {
      auto peremption = std::chrono::file_clock::now() - _scan_range;
      auto clean_oldest = [peremption](event_cont& to_clean,
                                       unsigned& event_counter) {
        for (auto event_iter = to_clean.begin();
             event_iter != to_clean.end();) {
          if (event_iter->second < peremption) {
            to_clean.erase(event_iter++);
          } else {
            ++event_iter;
          }
        }
      };
      clean_oldest(_warning, _nb_warning);
      clean_oldest(_critical, _nb_critical);
    }
  } catch (const std::exception& e) {
    SPDLOG_LOGGER_ERROR(_logger, "fail to read event: {}", e.what());
  }
}
