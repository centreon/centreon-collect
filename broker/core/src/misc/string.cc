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
