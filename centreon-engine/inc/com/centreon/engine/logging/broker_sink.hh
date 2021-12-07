/*
** Copyright 2011-2021 Centreon
**
** This file is part of Centreon Engine.
**
** Centreon Engine is free software: you can redistribute it and/or
** modify it under the terms of the GNU General Public License version 2
** as published by the Free Software Foundation.
**
** Centreon Engine is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
** General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with Centreon Engine. If not, see
** <http://www.gnu.org/licenses/>.
*/
#ifndef CCE_LOGGING_BROKER_SINK_HH
#define CCE_LOGGING_BROKER_SINK_HH
#include <spdlog/details/fmt_helper.h>
#include <spdlog/fmt/fmt.h>
#include <spdlog/sinks/base_sink.h>
#include <iostream>
#include <mutex>
#include "com/centreon/engine/broker.hh"
#include "com/centreon/engine/logging/broker.hh"
#include "com/centreon/engine/namespace.hh"
#include "com/centreon/unique_array_ptr.hh"
#include "spdlog/details/null_mutex.h"
CCE_BEGIN()

namespace logging {
template <typename Mutex>
class broker_sink : public spdlog::sinks::base_sink<Mutex> {
 protected:
  void sink_it_(const spdlog::details::log_msg& msg) override {
    // log_msg is a struct containing the log entry info like level, timestamp,
    // thread id etc. msg.raw contains pre formatted log

    // If needed (very likely but not mandatory), the sink formats the message
    // before sending it to its final destination:
    spdlog::memory_buf_t formatted;
    // spdlog::sinks::base_sink<Mutex>::formatter_->format(msg, formatted);
    spdlog::details::fmt_helper::append_string_view(msg.payload, formatted);
    std::string ret = fmt::to_string(formatted);
    // Event broker callback.
    unique_array_ptr<char> copy(new char[ret.size() + 1]);
    strncpy(copy.get(), ret.c_str(), ret.size());
    copy.get()[ret.size()] = 0;

    broker_log_data(NEBTYPE_LOG_DATA, NEBFLAG_NONE, NEBATTR_NONE, copy.get(),
                    10, time(NULL), NULL);
  }

  void flush_() override { std::cout << std::flush; }
};

using broker_sink_mt = broker_sink<std::mutex>;
using broker_sink_st = broker_sink<spdlog::details::null_mutex>;

}  // namespace logging
CCE_END()

#endif  // !CCE_LOGGING_BROKER_SINK_HH
