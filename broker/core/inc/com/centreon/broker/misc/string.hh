/**
 * Copyright 2013-2023 Centreon
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

#ifndef CCB_MISC_STRING_HH
#define CCB_MISC_STRING_HH

#include <fmt/format.h>

#include <cerrno>

namespace com::centreon::broker::misc::string {
inline std::string& replace(std::string& str,
                            std::string const& old_str,
                            std::string const& new_str) {
  std::size_t pos(str.find(old_str, 0));
  while (pos != std::string::npos) {
    str.replace(pos, old_str.size(), new_str);
    pos = str.find(old_str, pos + new_str.size());
  }
  return (str);
}

std::string& trim(std::string& str) noexcept;
bool is_number(const std::string& s);

std::string escape(const std::string& str, size_t s);

std::string debug_buf(const char* data, int32_t size, int max_len = 10);

}  // namespace com::centreon::broker::misc::string

#endif  // !CCB_MISC_STRING_HH
