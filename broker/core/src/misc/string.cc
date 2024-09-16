/**
 * Copyright 2011-2013 Centreon
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

#include "com/centreon/broker/misc/string.hh"
#include "com/centreon/common/utf8.hh"
#include "com/centreon/exceptions/msg_fmt.hh"

#include <fmt/format.h>

#include <cassert>

using namespace com::centreon::broker::misc;

static char const* whitespaces(" \t\r\n");

/**
 *  Trim a string.
 *
 *  @param[in] str The string.
 *
 *  @return The trimming stream.
 */
std::string& string::trim(std::string& str) noexcept {
  size_t pos(str.find_last_not_of(whitespaces));
  if (pos == std::string::npos)
    str.clear();
  else {
    str.erase(pos + 1);
    if ((pos = str.find_first_not_of(whitespaces)) != std::string::npos)
      str.erase(0, pos);
  }
  return str;
}

/**
 * @brief Encode a string to base64.
 *
 * @param str The string to encode.
 *
 * @return The base64 encoding string.
 */
std::string string::base64_encode(const std::string& str) {
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
std::string string::base64_decode(const std::string& ascdata) {
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

  const std::string::const_iterator last = ascdata.end();
  int bits_collected = 0;
  unsigned int accumulator = 0;

  for (std::string::const_iterator i = ascdata.begin(); i != last; ++i) {
    const int c = *i;
    if (std::isspace(c) || c == '=') {
      // Skip whitespace and padding. Be liberal in what you accept.
      continue;
    }
    if (c > 127 || c < 0 || reverse_table[c] > 63) {
      throw exceptions::msg_fmt(
          "This contains characters not legal in a base64 encoded string.");
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

bool string::is_number(const std::string& s) {
  return !s.empty() && std::find_if(s.begin(), s.end(), [](char c) {
                         return !std::isdigit(c);
                       }) == s.end();
}

/**
 * @brief Escape the given string so that it can be directly inserted into the
 * database. Essntially, characters \ and ' are prefixed with \. The function
 * also only keeps the s first characters.
 *
 * @param str the string to escape and truncate.
 * @param s The desired size.
 *
 * @return The resulting string.
 */
std::string string::escape(const std::string& str, size_t s) {
  size_t found = str.find_first_of("'\\");
  if (found == std::string::npos)
    return str.substr(0, common::adjust_size_utf8(str, s));
  else {
    std::string ret;
    /* ret is reserved with the worst size */
    ret.reserve(found + 2 * (str.size() - found));
    std::copy(str.data(), str.data() + found, std::back_inserter(ret));
    ret += '\\';
    ret += str[found];
    do {
      ++found;
      size_t ffound = str.find_first_of("'\\", found);
      if (ffound == std::string::npos) {
        std::copy(str.data() + found, str.data() + str.size(),
                  std::back_inserter(ret));
        break;
      }
      std::copy(str.data() + found, str.data() + ffound,
                std::back_inserter(ret));
      ret += '\\';
      ret += str[ffound];
      found = ffound;
    } while (found < s);
    ret.resize(common::adjust_size_utf8(ret, s));
    if (ret.size() > 1) {
      auto it = --ret.end();
      size_t nb{0};
      while (it != ret.begin() && *it == '\\') {
        --it;
        nb++;
      }
      if (it == ret.begin() && *it == '\\')
        nb++;
      if (nb & 1)
        ret.resize(ret.size() - 1);
    }
    return ret;
  }
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
std::string string::debug_buf(const char* data, int32_t size, int max_len) {
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
