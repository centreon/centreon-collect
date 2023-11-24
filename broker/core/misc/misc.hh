/**
 * Copyright 2015, 2023 Centreon
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

#ifndef CCB_MISC_MISC_HH
#define CCB_MISC_MISC_HH

#include <spdlog/spdlog.h>

#include <list>

#include "broker/core/misc/perfdata.hh"
#include "com/centreon/broker/multiplexing/muxer_filter.hh"

namespace com::centreon::broker {

namespace misc {
std::string temp_path();
std::list<std::string> split(std::string const& str, char sep);
uint16_t crc16_ccitt(char const* data, uint32_t data_len);
std::string exec(std::string const& cmd);
int32_t exec_process(char const** argv, bool wait_for_completion);
std::vector<char> from_hex(std::string const& str);
std::string dump_filters(const multiplexing::muxer_filter& filters);
std::list<perfdata> parse_perfdata(uint32_t host_id,
                                   uint32_t service_id,
                                   const char* str,
                                   std::shared_ptr<spdlog::logger> logger);
#if DEBUG_ROBOT
enum debug_object_instance {
  LOG_V2,
  STREAM_TCP_INPUT,
  STREAM_UNIFIED_SQL,
  DEBUG_OBJECT_SIZE,
};
void debug(const std::string& content);
void log_debug();
void add_debug(debug_object_instance idx);
void sub_debug(debug_object_instance idx);
void check_debug(debug_object_instance idx);
#endif
}  // namespace misc

#if DEBUG_ROBOT
#define DEBUG(content) misc::debug(content)
#define ADD_DEBUG(idx) misc::add_debug(idx)
#define SUB_DEBUG(idx) misc::sub_debug(idx)
#define CHECK_DEBUG(idx) misc::check_debug(idx)
#else
#define DEBUG(content)
#define ADD_DEBUG(idx)
#define SUB_DEBUG(idx)
#define CHECK_DEBUG(idx)
#endif
}  // namespace com::centreon::broker

#endif  // !CCB_MISC_MISC_HH
