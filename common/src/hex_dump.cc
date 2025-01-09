/**
 * Copyright 2023-2024 Centreon
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

#include "hex_dump.hh"

inline void char_to_hex(unsigned char c, std::string& output) noexcept {
  unsigned char val = c >> 4;
  output.push_back(val >= 0x0A ? ('A' + val - 0x0A) : ('0' + val));
  val = c & 0x0F;
  output.push_back(val >= 0x0A ? ('A' + val - 0x0A) : ('0' + val));
}

/**
 * @brief return a string in an hex format
 * format depends on nb_char_per_line
 * if nb_char_per_line <= 0 dump is only an hexa string
 * if nb_char_per_line > 0 dump is like 0000 xxxxxxxx abcd
 *
 * @param buffer
 * @param buff_len
 * @param nb_char_per_line
 * @return std::string
 */
std::string com::centreon::common::hex_dump(const unsigned char* buffer,
                                            size_t buff_len,
                                            uint32_t nb_char_per_line) {
  std::string ret;
  if (nb_char_per_line > 0) {
    size_t address_len = 0;
    size_t min = 1;
    while (min < buff_len) {
      min <<= 4;
      ++address_len;
    }
    size_t line_len = address_len + 1 /*space*/ +
                      2 * nb_char_per_line /*hexa*/ + 1 /*space*/ +
                      nb_char_per_line + 1 /*\n*/;
    ret.reserve(
        line_len * (buff_len + nb_char_per_line - 1) / nb_char_per_line + 1);

    unsigned current_address = 0;

    std::string address_format = "{:0" + std::to_string(address_len) + "X} ";

    for (const unsigned char *current = buffer, *end = buffer + buff_len;
         current < end; current_address += nb_char_per_line) {
      ret += fmt::format(address_format, current_address);
      std::string char_part;
      char_part.reserve(nb_char_per_line + 1);

      for (unsigned col = 0; col < nb_char_per_line && current < end;
           ++col, ++current) {
        char_to_hex(*current, ret);
        if (*current >= ' ' && *current <= '~') {
          char_part.push_back(*current);
        } else {
          char_part.push_back('.');
        }
      }
      ret.push_back(' ');
      for (uint32_t pad = char_part.length(); pad < nb_char_per_line; ++pad) {
        ret.push_back(' ');
        ret.push_back(' ');
      }
      ret += char_part;
      ret.push_back('\n');
    }

  } else {
    ret.reserve(nb_char_per_line * 2 + 1);
    for (const unsigned char *current = buffer, *end = buffer + buff_len;
         current < end; ++current) {
      char_to_hex(*current, ret);
    }
  }

  return ret;
}

/**
 * @brief This function is an internal function just used to debug. It displays
 * the data array as hex 8 bits integers in the limit of 20 values. If the array
 * is longer, only the 10 first bytes and the 10 last bytes are displayed.
 *
 * @param data A const char* array.
 * @param size The size of the data array.
 * @param max_len max dumping size
 * xxxxxxxxxxxxxxxxxxxxx....xxxxxxxxxxxxxxxxxxxxx max_len bytes max_len bytes
 * @return A string containing the result.
 */
std::string com::centreon::common::debug_buf(const char* data,
                                             int32_t size,
                                             int max_len) {
  auto to_str = [](uint8_t d) -> uint8_t {
    uint8_t ret;
    if (d < 10)
      ret = '0' + d;
    else
      ret = 'a' + d - 10;
    return ret;
  };

  std::string retval;
  int l1;
  if (size <= max_len)
    l1 = size;
  else
    l1 = max_len;

  for (int i = 0; i < l1; i++) {
    uint8_t c = data[i];
    uint8_t d1 = c >> 4;
    uint8_t d2 = c & 0xf;
    retval.push_back(to_str(d1));
    retval.push_back(to_str(d2));
  }
  if (size > max_len) {
    if (size > 2 * max_len)
      retval += "...";
    for (int i = std::max(size - max_len, l1); i < size; i++) {
      uint8_t c = data[i];
      uint8_t d1 = c >> 4;
      uint8_t d2 = c & 0xf;
      retval.push_back(to_str(d1));
      retval.push_back(to_str(d2));
    }
  }
  return retval;
}
