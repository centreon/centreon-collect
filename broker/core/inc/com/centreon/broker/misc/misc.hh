/*
** Copyright 2015 Centreon
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
**
** For more information : contact@centreon.com
*/

#ifndef CCB_MISC_MISC_HH
#define CCB_MISC_MISC_HH

#include "com/centreon/broker/misc/perfdata.hh"
#include "com/centreon/broker/multiplexing/muxer_filter.hh"
#include "com/centreon/broker/namespace.hh"

CCB_BEGIN()

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
void debug(const std::string& content);
#endif
}  // namespace misc

#if DEBUG_ROBOT
#define DEBUG(content) misc::debug(content)
#else
#define DEBUG(content)
#endif
CCB_END()

#endif  // !CCB_MISC_MISC_HH
