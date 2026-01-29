/**
 * Copyright 2026 Centreon
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

#include "com/centreon/broker/event_script/stream.hh"
#include <google/protobuf/util/message_differencer.h>
#include "com/centreon/broker/exceptions/config.hh"
#include "com/centreon/broker/exceptions/shutdown.hh"
#include "com/centreon/common/pool.hh"
#include "com/centreon/common/process/process.hh"
#include "common/log_v2/log_v2.hh"

using namespace com::centreon::broker;
using namespace com::centreon::broker::event_script;
namespace log_v2 = com::centreon::common::log_v2;

static std::string serialize_deterministic(
    const google::protobuf::Message& message) {
  std::string output;
  google::protobuf::io::StringOutputStream string_stream(&output);
  google::protobuf::io::CodedOutputStream coded_stream(&string_stream);

  coded_stream.SetSerializationDeterministic(true);

  message.SerializeToCodedStream(&coded_stream);
  return output;
}

bool io_data_compare::operator()(
    const std::shared_ptr<io::protobuf_base>& left,
    const std::shared_ptr<io::protobuf_base>& right) const {
  return google::protobuf::util::MessageDifferencer::Equals(*left->msg(),
                                                            *right->msg());
}

size_t io_data_compare::operator()(
    const std::shared_ptr<io::protobuf_base>& to_hash) const {
  static absl::Hash<std::string> hasher;
  return hasher(serialize_deterministic(*to_hash->msg()));
}

stream::stream(const std::string_view& script_path,
               const std::chrono::system_clock::duration& managed_event_ttl,
               const std::chrono::system_clock::duration& timeout)
    : io::stream("event_script"),
      _managed_event_ttl(managed_event_ttl),
      _timeout(timeout),
      _logger(log_v2::log_v2::instance().get(log_v2::log_v2::EVENT_SCRIPT)),
      _to_ack(std::make_shared<std::atomic_uint>(0)) {
  _script_cmdline = common::process<true>::parse_cmd_line(script_path);
}

/**
 *  Read
 *
 *  @param[out] d         Cleared.
 *  @param[in]  deadline  Timeout.
 *
 *  @return This method will throw.
 */
bool stream::read(std::shared_ptr<io::data>&, time_t) {
  throw exceptions::shutdown("cannot read from event_script");
}

int stream::write(std::shared_ptr<io::data> const& d) {
  std::shared_ptr<io::protobuf_base> pb_event =
      std::dynamic_pointer_cast<io::protobuf_base>(d);

  if (!pb_event) {
    // output is bad configured, it only deals with pb events
    throw exceptions::config(
        "event_script: configured event is not a protobuf event {}", *d);
  }

  std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
  std::chrono::system_clock::time_point perempt = now - _managed_event_ttl;
  if (_events.empty()) {
    auto& index = _events.get<1>();
    auto to_test = index.begin();
    while (!_events.empty() && to_test->inserted < perempt) {
      to_test = index.erase((to_test));
    }
  }

  if (_events.insert({pb_event, now}).second) {
    std::string json_dump;
    [[maybe_unused]] auto dummy = google::protobuf::util::MessageToJsonString(
        *pb_event->msg(), &json_dump);

    com::centreon::common::process_args::pointer cmdline =
        std::make_shared<com::centreon::common::process_args>(*_script_cmdline);

    cmdline->add_arg(json_dump);

    std::shared_ptr<common::process<true>> proc =
        std::make_shared<common::process<true>>(common::pool::io_context_ptr(),
                                                _logger, cmdline, true, false,
                                                nullptr);

    proc->start_process(
        [to_ack = _to_ack](const common::process<true>& proc, int exit_code,
                           common::e_exit_status exit_status,
                           const std::string& std_out,
                           const std::string& std_err) {
          if (exit_status == common::e_exit_status::normal && !exit_code) {
            to_ack->fetch_add(1);
          } else {
            SPDLOG_LOGGER_ERROR(proc.get_logger(), "fail to execute {}: {}",
                                proc.get_exe_path(), std_err);
          }
        },
        _timeout);

  } else {
    SPDLOG_LOGGER_DEBUG(_logger, "event ignored: {}", *d);
  }

  return _to_ack->exchange(0);
}

/**
 * @brief get number of event passed to script to acknowledge
 *
 * @return int32_t number of event passed to script
 */
int32_t stream::stop() {
  return _to_ack->exchange(0);
}
