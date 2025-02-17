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

#pragma comment(lib, "wevtapi.lib")

#include "check_event_log.hh"
#include "com/centreon/common/rapidjson_helper.hh"
#include "com/centreon/exceptions/msg_fmt.hh"

using namespace com::centreon::agent;
using namespace com::centreon::agent::check_event_log_detail;

namespace com::centreon::agent::check_event_log_detail {

/*event::event(EVT_HANDLE render_context, EVT_HANDLE event_handle) {
  DWORD buffer_size = 0;
  DWORD buffer_used = 0;
  DWORD property_count = 0;

  // Get the size of the buffer needed.
  if (!EvtRender(render_context, event_handle, EvtRenderEventValues,
                 buffer_size, nullptr, &buffer_used, &property_count)) {
    if (GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
      buffer_size = buffer_used;
    } else {
      throw exceptions::msg_fmt("Failed to render event: {}", GetLastError());
    }
  }

  std::unique_ptr<uint8_t[]> buffer(new uint8_t[buffer_size]);

  // Render the event.
  if (!EvtRender(render_context, event_handle, EvtRenderEventValues,
                 buffer_size, buffer.get(), &buffer_used, &property_count)) {
    throw exceptions::msg_fmt("Failed to render event: {}", GetLastError());
  }

  // Process the rendered event values.
  PEVT_VARIANT values = reinterpret_cast<PEVT_VARIANT>(buffer.get());
  // Example: Accessing the event message
  /*if (values[EvtSystemMessage].Type == EvtVarTypeString) {
    std::wstring message = values[EvtSystemMessage].StringVal;
    // Do something with the message
  }
}*/

static const absl::flat_hash_map<std::string_view, uint64_t> _str_to_keywords{
    {"auditsuccess", _keywords_audit_success},
    {"auditfailure", _keywords_audit_failure},
};

static const absl::flat_hash_map<std::string_view, uint8_t> _str_to_levels{
    {"critical", 1},      {"error", 2},       {"warning", 3},
    {"warn", 3},          {"information", 4}, {"info", 4},
    {"informational", 4}, {"verbose", 5},     {"debug", 5}};

}  // namespace com::centreon::agent::check_event_log_detail

event_data::event_data(EVT_HANDLE render_context, EVT_HANDLE event_handle) {
  DWORD buffer_size = 0;
  DWORD buffer_used = 0;
  _property_count = 0;

  // Get the size of the buffer needed.
  if (!EvtRender(render_context, event_handle, EvtRenderEventValues,
                 buffer_size, nullptr, &buffer_used, &_property_count)) {
    if (GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
      buffer_size = buffer_used;
    } else {
      throw exceptions::msg_fmt("Failed to render event: {}", GetLastError());
    }
  }

  _data = std::unique_ptr<uint8_t[]>(new uint8_t[buffer_size]);

  // Render the event.
  if (!EvtRender(render_context, event_handle, EvtRenderEventValues,
                 buffer_size, _data.get(), &buffer_used, &_property_count)) {
    throw exceptions::msg_fmt("Failed to render event: {}", GetLastError());
  }
}

std::wstring_view event_data::get_provider() const {
  if (_property_count > EvtSystemProviderName) {
    return reinterpret_cast<EVT_VARIANT*>(_data.get())[EvtSystemProviderName]
        .StringVal;
  } else {
    return L"";
  }
}

uint16_t event_data::get_event_id() const {
  if (_property_count > EvtSystemEventID) {
    return reinterpret_cast<EVT_VARIANT*>(_data.get())[EvtSystemEventID]
        .UInt16Val;
  } else {
    return 0;
  }
}

uint8_t event_data::get_level() const {
  if (_property_count > EvtSystemLevel) {
    return reinterpret_cast<EVT_VARIANT*>(_data.get())[EvtSystemLevel].ByteVal;
  } else {
    return 0;
  }
}

uint16_t event_data::get_task() const {
  if (_property_count > EvtSystemTask) {
    return reinterpret_cast<EVT_VARIANT*>(_data.get())[EvtSystemTask].UInt16Val;
  } else {
    return 0;
  }
}

int64_t event_data::get_keywords() const {
  if (_property_count > EvtSystemKeywords) {
    return reinterpret_cast<EVT_VARIANT*>(_data.get())[EvtSystemKeywords]
        .Int64Val;
  } else {
    return 0;
  }
}

uint64_t event_data::get_time_created() const {
  if (_property_count > EvtSystemTimeCreated) {
    return reinterpret_cast<EVT_VARIANT*>(_data.get())[EvtSystemTimeCreated]
        .FileTimeVal;
  } else {
    return 0;
  }
}

uint64_t event_data::get_record_id() const {
  if (_property_count > EvtSystemEventRecordId) {
    return reinterpret_cast<EVT_VARIANT*>(_data.get())[EvtSystemEventRecordId]
        .UInt64Val;
  } else {
    return 0;
  }
}

std::wstring_view event_data::get_computer() const {
  if (_property_count > EvtSystemComputer) {
    return reinterpret_cast<EVT_VARIANT*>(_data.get())[EvtSystemComputer]
        .StringVal;
  } else {
    return L"";
  }
}

std::wstring_view event_data::get_channel() const {
  if (_property_count > EvtSystemChannel) {
    return reinterpret_cast<EVT_VARIANT*>(_data.get())[EvtSystemChannel]
        .StringVal;
  } else {
    return L"";
  }
}

event_filter::event_filter(const std::string_view& filter_str,
                           const std::shared_ptr<spdlog::logger>& logger)
    : _logger(logger) {
  _filter = filter::create_filter(filter_str, logger, true);
  if (!_filter) {
    SPDLOG_LOGGER_ERROR(_logger, "fail to parse filter string: {}", filter_str);
  }
}

check_event_log::check_event_log(
    const std::shared_ptr<asio::io_context>& io_context,
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
    }
  } catch (const std::exception& e) {
    SPDLOG_LOGGER_ERROR(_logger, "check_uptime, fail to parse arguments: {}",
                        e.what());
    throw;
  }
}

/**
 * @brief get uptime with GetTickCount64
 *
 * @param timeout unused
 */
void check_event_log::start_check([[maybe_unused]] const duration& timeout) {
  if (!_start_check(timeout)) {
    return;
  }
  std::string output;
  common::perfdata perf;
  e_status status = compute(GetTickCount64(), &output, &perf);

  _io_context->post([me = shared_from_this(), this, out = std::move(output),
                     status, performance = std::move(perf)]() {
    on_completion(_get_running_check_index(), status, {performance}, {out});
  });
}

e_status check_event_log::compute(uint64_t ms_uptime,
                                  std::string* output,
                                  com::centreon::common::perfdata* perf) {
  return e_status::ok;
}
