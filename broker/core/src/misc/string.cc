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

#include <fmt/format.h>

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
