/**
 * Copyright 2011-2025 Centreon
 *
 * This file is part of Centreon Engine.
 *
 * Centreon Engine is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation.
 *
 * Centreon Engine is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Centreon Engine. If not, see
 * <http://www.gnu.org/licenses/>.
 */
#ifndef CCE_LOGGING_BROKER_SINK_HH
#define CCE_LOGGING_BROKER_SINK_HH
#include <spdlog/details/fmt_helper.h>
#include <spdlog/sinks/base_sink.h>
#include "com/centreon/engine/broker.hh"
#include "com/centreon/engine/logging/broker.hh"
#include "com/centreon/engine/logging/logger.hh"
#include "com/centreon/engine/nebstructs.hh"
#include "com/centreon/unique_array_ptr.hh"
#include "spdlog/details/null_mutex.h"
namespace com::centreon::engine::logging {

template <typename Mutex>
class broker_sink : public spdlog::sinks::base_sink<Mutex> {
 protected:
  void sink_it_(const spdlog::details::log_msg& msg) override {
    // log_msg is a struct containing the log entry info like level, timestamp,
    // thread id etc. msg.raw contains pre formatted log

    // If needed (very likely but not mandatory), the sink formats the message
    // before sending it to its final destination:
    if (this->should_log(msg.level)) {
      std::string message{fmt::to_string(msg.payload)};

      // Make callbacks.
      broker_log_data(message.c_str(), time(nullptr));
    }
  }

  void flush_() override {}
};

using broker_sink_mt = broker_sink<std::mutex>;
using broker_sink_st = broker_sink<spdlog::details::null_mutex>;

}  // namespace com::centreon::engine::logging

#endif  // !CCE_LOGGING_BROKER_SINK_HH
