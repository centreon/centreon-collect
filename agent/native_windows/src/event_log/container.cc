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

#include "boost/multi_index_container.hpp"
#include "boost/tuple/tuple.hpp"
#include "event_log/uniq.hh"
#include "windows_util.hh"

#include "event_log/container.hh"

using namespace com::centreon::agent;
using namespace com::centreon::agent::event_log;

event_container::event_container(const std::string_view& file,
                                 const std::string_view& unique_str,
                                 const std::string_view& primary_filter,
                                 const std::string_view& warning_filter,
                                 const std::string_view& critical_filter,
                                 duration scan_range,
                                 bool need_to_decode_message_content,
                                 const std::shared_ptr<spdlog::logger>& logger)
    : _scan_range(scan_range),
      _warning_scan_range(scan_range),
      _critical_scan_range(scan_range),
      _need_to_decode_message_content(need_to_decode_message_content),
      _file(file.begin(), file.end()),
      _event_compare(unique_str, logger),
      _container_initializer(boost::make_tuple(
          boost::make_tuple(
              0,
              boost::multi_index::const_mem_fun<
                  com::centreon::agent::event_log::unique_event,
                  const com::centreon::agent::event_log::event&,
                  &com::centreon::agent::event_log::unique_event::get_event>(),
              _event_compare,
              _event_compare),
          oldest_index_type::ctor_args())),
      _critical(_container_initializer),
      _warning(_container_initializer),
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
  } else {
    _primary_filter = std::make_unique<event_filter>(
        "written > 60m and level in ('error', 'warning', 'critical')", logger);
  }

  if (!warning_filter.empty()) {
    try {
      _warning_filter = std::make_unique<event_filter>(warning_filter, logger);
      duration written_limit = _warning_filter->get_written_limit();
      if (written_limit.count() && written_limit < _warning_scan_range) {
        _warning_scan_range = written_limit;
      }
    } catch (const std::exception& e) {
      SPDLOG_LOGGER_ERROR(logger, "fail to parse warning filter: {}", e.what());
      throw;
    }
  } else {
    _warning_filter =
        std::make_unique<event_filter>("level == 'warning'", logger);
  }

  if (!critical_filter.empty()) {
    try {
      _critical_filter =
          std::make_unique<event_filter>(critical_filter, logger);
      duration written_limit = _critical_filter->get_written_limit();
      if (written_limit.count() && written_limit < _critical_scan_range) {
        _critical_scan_range = written_limit;
      }
    } catch (const std::exception& e) {
      SPDLOG_LOGGER_ERROR(logger, "fail to parse critical filter: {}",
                          e.what());
      throw;
    }
  } else {
    _critical_filter = std::make_unique<event_filter>(
        "level in ('error', 'critical')", logger);
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
  query = std::format(L"Event/System[TimeCreated[timediff(@SystemTime) <= {}]]",
                      std::chrono::duration_cast<std::chrono::milliseconds>(
                          std::max(_warning_scan_range, _critical_scan_range))
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
    std::string message;
    if (_critical_filter->allow(raw_event)) {
      event_status = e_status::critical;
    } else if (_warning_filter->allow(raw_event)) {
      event_status = e_status::warning;
    } else {
      _ok_events.emplace(
          std::chrono::file_clock::duration(raw_event.get_time_created()));
    }

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

    auto add_event = [&](event_cont& cont) {
      auto yet_exist = cont.find(to_ins);
      if (yet_exist != cont.end()) {
        if (yet_exist->get_oldest() <
            to_ins.time()) {  // we modify without recalculate index
          yet_exist->add_time_point(to_ins.time());
        } else {
          cont.modify(yet_exist, [tp = to_ins.time()](unique_event& to_modify) {
            to_modify.add_time_point(tp);
          });
        }
      } else {
        cont.emplace(std::move(to_ins));
      }
    };

    if (event_status == e_status::warning) {
      ++_nb_warning;
      add_event(_warning);
    } else {
      ++_nb_critical;
      add_event(_critical);
    }

    // every 10 events we clean oldest events
    if (!((++_insertion_cpt) % 10)) {
      clean_perempted_events();
    }
  } catch (const std::exception& e) {
    SPDLOG_LOGGER_ERROR(_logger, "fail to read event: {}", e.what());
  }
}

void event_container::clean_perempted_events() {
  auto clean_oldest = [](event_cont& to_clean, unsigned& event_counter,
                         auto peremption) {
    auto& ind = to_clean.get<1>();
    auto oldest = ind.begin();
    while (!to_clean.empty() && oldest->get_oldest() < peremption) {
      ind.modify(oldest, [peremption](unique_event& evt) {
        evt.clean_oldest(peremption);
      });
      if (oldest->empty()) {
        ind.erase(oldest);
      }
      if (!ind.empty()) {
        oldest = ind.begin();
      }
    }
  };
  auto now = std::chrono::file_clock::now();
  clean_oldest(_warning, _nb_warning, now - _warning_scan_range);
  clean_oldest(_critical, _nb_critical, now - _critical_scan_range);

  if (!_ok_events.empty()) {
    auto limit = _ok_events.lower_bound(now - _scan_range);
    _ok_events.erase(_ok_events.begin(), limit);
  }
}
