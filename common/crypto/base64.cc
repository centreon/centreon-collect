/**
 * Copyright 2024 Centreon
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
#include "base64.hh"
#include "com/centreon/exceptions/msg_fmt.hh"

using com::centreon::exceptions::msg_fmt;

namespace com::centreon::common::crypto {
/**
 * @brief Encode a string to base64.
 *
 * @param str The string to encode.
 *
 * @return The base64 encoding string.
 */
std::string base64_encode(const std::string_view& str) {
  const constexpr std::string_view b(
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/");
  std::string retval;
  retval.reserve((str.size() / 3 + (str.size() % 3 > 0)) * 4);

  int val = 0, valb = -6;
  for (unsigned char c : str) {
    val = (val << 8) + c;
    valb += 8;
    while (valb >= 0) {
      retval.push_back(b[(val >> valb) & 0x3f]);
      valb -= 6;
    }
  }
  if (valb > -6)
    retval.push_back(b[((val << 8) >> (valb + 8)) & 0x3f]);
  while (retval.size() % 4)
    retval.push_back('=');

  return retval;
}

/**
 * @brief Decode a base64 string. The result is stored into a string.
 *
 * @param ascdata A string base64 encoded.
 *
 * @return The decoded string.
 */
std::string base64_decode(const std::string_view& ascdata) {
  const constexpr std::array<char, 128> reverse_table{
      64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
      64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
      64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 62, 64, 64, 64, 63,
      52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 64, 64, 64, 64, 64, 64,
      64, 0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14,
      15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 64, 64, 64, 64, 64,
      64, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
      41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 64, 64, 64, 64, 64};

  std::string retval;

  const auto last = ascdata.end();
  int bits_collected = 0;
  unsigned int accumulator = 0;

  for (auto i = ascdata.begin(); i != last; ++i) {
    const int c = *i;
    if (std::isspace(c) || c == '=') {
      // Skip whitespace and padding. Be liberal in what you accept.
      continue;
    }
    if (c > 127 || c < 0 || reverse_table[c] > 63) {
      throw msg_fmt(
          "This string '{}' contains characters not legal in a base64 encoded "
          "string.",
          ascdata);
    }
    accumulator = (accumulator << 6) | reverse_table[c];
    bits_collected += 6;
    if (bits_collected >= 8) {
      bits_collected -= 8;
      retval += static_cast<char>((accumulator >> bits_collected) & 0xffu);
    }
  }

  return retval;
}
}  // namespace com::centreon::common::crypto
