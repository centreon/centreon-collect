/**
 * Copyright 2011-2014, 2017, 2020-2022 Centreon (https://www.centreon.com/)
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

#include <absl/strings/numbers.h>

#include "com/centreon/engine/string.hh"

#include "com/centreon/engine/exceptions/error.hh"

using namespace com::centreon::engine;

static char const* whitespaces(" \t\r\n");

/**
 *  Get key and value from line.
 *
 *  @param[in,out] line  The line to process.
 *  @param[out]    key   The key pointer.
 *  @param[out]    value The value pointer.
 *  @param[in]     delim The delimiter.
 */
bool string::split(std::string& line,
                   char const** key,
                   char const** value,
                   char delim) {
  std::size_t delim_pos(line.find_first_of(delim));
  if (delim_pos == std::string::npos)
    return false;

  std::size_t first_pos;
  std::size_t last_pos;
  line.append("", 1);

  last_pos = line.find_last_not_of(whitespaces, delim_pos - 1);
  if (last_pos == std::string::npos)
    *key = NULL;
  else {
    first_pos = line.find_first_not_of(whitespaces);
    line[last_pos + 1] = '\0';
    *key = line.data() + first_pos;
  }

  first_pos = line.find_first_not_of(whitespaces, delim_pos + 1);
  if (first_pos == std::string::npos)
    *value = NULL;
  else {
    last_pos = line.find_last_not_of(whitespaces);
    line[last_pos + 1] = '\0';
    *value = line.data() + first_pos;
  }

  return true;
}

std::string string::extract_perfdata(std::string const& perfdata,
                                     std::string const& metric) noexcept {
  size_t pos, pos_start = 0;

  do {
    pos_start = perfdata.find(metric, pos_start);
    pos = pos_start;

    // Metric name not found
    if (pos == std::string::npos)
      return "";

    while (pos > 0 && perfdata[pos - 1] != ' ')
      pos--;

    size_t end = pos + metric.size();
    while (end < perfdata.size() && perfdata[end] != '=')
      end++;

    // Metric name should be from pos to end. We have to verify this

    // Are there quotes?
    size_t p = pos;
    size_t e = end - 1;
    if (perfdata[p] == '\'' && perfdata[e] == '\'') {
      p++;
      e--;
    }

    // Is the metric type specified?
    char c1 = perfdata[p], c2 = perfdata[p + 1], e1 = perfdata[e];
    if (c2 == '[' && e1 == ']' && (c1 == 'a' || c1 == 'd' || c1 == 'g')) {
      p += 2;
      e--;
    }
    if (e - p + 1 == metric.size()) {
      size_t ee = perfdata.find_first_of(" \n\r", end);
      return perfdata.substr(pos, ee - pos);
    }
    pos_start++;
  } while (pos < perfdata.size());
  return "";
}

std::string& string::remove_thresholds(std::string& perfdata) noexcept {
  size_t pos1 = perfdata.find(";");

  if (pos1 == std::string::npos)
    // No ';' so no thresholds in this perfdata
    return perfdata;

  size_t pos2 = perfdata.find(";", pos1 + 1);
  if (pos2 == std::string::npos) {
    // No second threshold. We just have to remove the first one.
    perfdata.resize(pos1);
    return perfdata;
  }

  size_t pos3 = perfdata.find(";", pos2 + 1);
  if (pos3 == std::string::npos) {
    // No min/max. We just have to remove thresholds.
    perfdata.resize(pos1);
    return perfdata;
  }

  perfdata.replace(pos1, pos3 - pos1, ";;");
  return perfdata;
}

/**
 * @brief extract a part of the string_view passed in the construtor
 * it allows empty field as my_strtok
 *
 * @param sep separator
 * @param extracted field
 * @return true extracted is valid
 * @return false current pos is yet beyond string end
 */
bool string::c_strtok::extract(char sep, std::string_view& extracted) {
  if (_pos == std::string_view::npos) {
    return false;
  }
  size_type old_pos = _pos;
  _pos = _src.find(sep, old_pos);
  if (_pos != std::string_view::npos) {
    extracted = _src.substr(old_pos, (_pos++) - old_pos);
  } else {
    extracted = _src.substr(old_pos);
  }
  return true;
}

/**
 * @brief extract a part of the string_view passed in the construtor
 * it allows empty field as my_strtok
 * if sep is not found it returns part from the current position to the end
 * if current pos is yet beyond string end, it returns std::nullopt
 *
 * @param sep separator
 * @return std::string_view field extracted
 */
std::optional<std::string_view> string::c_strtok::extract(char sep) {
  std::string_view ret;
  if (!extract(sep, ret)) {
    return std::nullopt;
  }
  return ret;
}

/**
 * @brief extract a part of the string_view passed in the construtor
 * it allows empty field as my_strtok
 *
 * @param sep separator
 * @param extracted field
 * @return true extracted is valid
 * @return false current pos is yet beyond string end
 */
bool string::c_strtok::extract(char sep, std::string& extracted) {
  std::string_view ret;
  if (!extract(sep, ret)) {
    return false;
  }
  extracted.assign(ret.begin(), ret.end());
  return true;
}

bool string::c_strtok::extract(char sep, int& extracted) {
  std::string_view ret;
  if (!extract(sep, ret)) {
    return false;
  }
  if (absl::SimpleAtoi(ret, &extracted)) {
    return true;
  }
  _pos = std::string_view::npos;
  return false;
}

void string::unescape(std::string& str) {
  size_t read = str.find('\\');
  if (read == std::string::npos)
    return;

  size_t write = read;
  const size_t len = str.size();
  while (read < len) {
    if (str[read] != '\\' || read + 1 == len) {
      str[write++] = str[read++];
      continue;
    }
    switch (str[read + 1]) {
      case 'n':
        str[write++] = '\n';
        break;
      case 'r':
        str[write++] = '\r';
        break;
      case 't':
        str[write++] = '\t';
        break;
      case '\\':
        str[write++] = '\\';
        break;
      default:
        str[write++] = '\\';
        str[write++] = str[read + 1];
        break;
    }
    read += 2;
  }
  str.resize(write);
}
