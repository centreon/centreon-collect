/**
 * Copyright 2024 Centreon (https://www.centreon.com/)
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * For more information : contact@centreon.com
 *
 */

#ifndef CCCM_HEX_DUMP_HH
#define CCCM_HEX_DUMP_HH

namespace com::centreon::common {

std::string hex_dump(const unsigned char* buffer,
                     size_t buff_len,
                     uint32_t nb_char_per_line);

inline std::string hex_dump(const std::string& buffer,
                            uint32_t nb_char_per_line) {
  return hex_dump(reinterpret_cast<const unsigned char*>(buffer.data()),
                  buffer.size(), nb_char_per_line);
}

std::string debug_buf(const char* data, int32_t size, int max_len = 10);

}  // namespace com::centreon::common

#endif
