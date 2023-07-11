/*
** Copyright 2023 Centreon
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

#include <fmt/format.h>

#include "com/centreon/common/hex_dump.hh"

inline void char_to_hex(unsigned char c, std::string& output) noexcept {
  unsigned char val = c >> 4;
  output.push_back(val >= 0x0A ? ('A' + val - 0x0A) : ('0' + val));
  val = c & 0x0F;
  output.push_back(val >= 0x0A ? ('A' + val - 0x0A) : ('0' + val));
}

/**
 * @brief return a string in an hex format
 * format depends on nb_char_per_line
 * if nb_char_per_line <= 0 dump is only an haxa string
 * if nb_char_per_line > 0 dump is like 0000 xxxxxxxx abcd
 *
 * @param buffer
 * @param buff_len
 * @param nb_char_per_line
 * @return std::string
 */
std::string com::centreon::common::hex_dump(const unsigned char* buffer,
                                            size_t buff_len,
                                            int nb_char_per_line) {
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
      for (unsigned pad = char_part.length(); pad < nb_char_per_line; ++pad) {
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
