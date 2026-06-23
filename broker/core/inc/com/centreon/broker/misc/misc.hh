/*
 * Copyright 2015-2023 Centreon
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

#include "com/centreon/broker/multiplexing/muxer_filter.hh"

namespace com::centreon::broker::misc {
std::string temp_path();
std::list<std::string> split(std::string const& str, char sep);
uint16_t crc16_ccitt(char const* data, uint32_t data_len);
int32_t exec_process(char const** argv, bool wait_for_completion);
std::vector<char> from_hex(std::string const& str);
std::string dump_filters(const multiplexing::muxer_filter& filters);

/**
 * @brief execute a command via popen(shell)
 * This trick makes that this function can be used only with constant strings
 * @tparam N
 * @param cmd   cmd to execute
 * @return std::string
 */
template <std::size_t N>
std::string exec(const char (&cmd)[N]) {
  std::array<char, 128> buffer;
  std::string result;
  std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd, "r"), pclose);
  if (!pipe)
    throw std::runtime_error("popen() failed!");

  while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
    result += buffer.data();
  }
  return result;
}

#if DEBUG_ROBOT
void debug(const std::string& content);
#endif

#if DEBUG_ROBOT
#define DEBUG(content) debug(content)
#else
#define DEBUG(content)
#endif
}  // namespace com::centreon::broker::misc

#endif  // !CCB_MISC_MISC_HH
