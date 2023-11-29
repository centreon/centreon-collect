/*
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

#include "absl/strings/ascii.h"
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

/**
 *  Get key and value from line.
 *
 *  @param[in]  line  The line to extract data.
 *  @param[out] key   The key to fill.
 *  @param[out] value The value to fill.
 *  @param[in]  delim The delimiter.
 */
bool string::split(std::string const& line,
                   std::string& key,
                   std::string& value,
                   char delim) {
  std::size_t delim_pos(line.find_first_of(delim));
  if (delim_pos == std::string::npos)
    return false;

  std::size_t first_pos;
  std::size_t last_pos;

  last_pos = line.find_last_not_of(whitespaces, delim_pos - 1);
  if (last_pos == std::string::npos)
    key.clear();
  else {
    first_pos = line.find_first_not_of(whitespaces);
    key.assign(line, first_pos, last_pos + 1 - first_pos);
  }

  first_pos = line.find_first_not_of(whitespaces, delim_pos + 1);
  if (first_pos == std::string::npos)
    value.clear();
  else {
    last_pos = line.find_last_not_of(whitespaces);
    value.assign(line, first_pos, last_pos + 1 - first_pos);
  }

  return true;
}

/**
 *  Split data into sorted elements.
 *
 *  @param[in]  data   The data to split.
 *  @param[out] out    The set to fill.
 *  @param[in]  delim  The delimiter.
 */
void string::split(std::string const& data,
                   std::set<std::string>& out,
                   char delim) {
  auto elements = absl::StrSplit(data, delim);
  for (auto e : elements) {
    e = absl::StripAsciiWhitespace(e);
    out.emplace(e.data(), e.size());
  }
}

/**
 *  Split data into pair of sorted elements.
 *
 *  @param[in]  data   The data to split.
 *  @param[out] out    The set to fill.
 *  @param[in]  delim  The delimiter.
 */
void string::split(std::string const& data,
                   std::set<std::pair<std::string, std::string> >& out,
                   char delim) {
  auto elements = absl::StrSplit(data, delim);
  for (auto it = elements.begin(), end = elements.end(); it != end; ++it) {
    auto first = it++;
    if (it == end)
      throw engine_error() << "Not enough elements in the line to make pairs";
    std::string_view k = absl::StripAsciiWhitespace(*first);
    std::string_view v = absl::StripAsciiWhitespace(*it);
    out.insert(std::make_pair(std::string(k.data(), k.size()),
                              std::string(v.data(), v.size())));
  }
}

/**
 *  Trim at the left of the string.
 *
 *  @param[in] str The string.
 *
 *  @return The trimming stream.
 */
std::string& string::trim_left(std::string& str) noexcept {
  size_t pos(str.find_first_not_of(whitespaces));
  if (pos != std::string::npos)
    str.erase(0, pos);
  return str;
}

/**
 *  Trim at the right of the string.
 *
 *  @param[in] str The string.
 *
 *  @return The trimming stream.
 */
std::string& string::trim_right(std::string& str) noexcept {
  size_t pos(str.find_last_not_of(whitespaces));
  if (pos == std::string::npos)
    str.clear();
  else
    str.erase(pos + 1);
  return str;
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
 * if current pos is yet beyond string end, it returns boost::none
 *
 * @param sep separator
 * @return std::string_view field extracted
 */
absl::optional<std::string_view> string::c_strtok::extract(char sep) {
  std::string_view ret;
  if (!extract(sep, ret)) {
    return absl::nullopt;
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

/**
 * @brief Unescape the string buffer. Works with \t, \n, \r and \\. The buffer
 * is directly changed. No copy is made.
 *
 * @param buffer
 */
void string::unescape(char* buffer) {
  if (buffer == nullptr)
    return;
  char* read_pos = strchrnul(buffer, '\\');
  char* prev_read_pos = nullptr;
  while (*read_pos) {
    char c = read_pos[1];
    if (c == 'n' || c == 'r' || c == 't' || c == '\\') {
      if (prev_read_pos) {
        size_t len = read_pos - prev_read_pos;
        memmove(buffer, prev_read_pos, len);
        buffer += len;
      } else
        buffer = read_pos;

      prev_read_pos = read_pos + 2;

      switch (c) {
        case 'n':
          *buffer = '\n';
          break;
        case 'r':
          *buffer = '\r';
          break;
        case 't':
          *buffer = '\t';
          break;
        case '\\':
          *buffer = '\\';
          break;
      }
      ++buffer;
    } else if (read_pos[1] == 0)
      break;
    read_pos = strchrnul(read_pos + 2, '\\');
  }
  if (prev_read_pos) {
    size_t len = read_pos - prev_read_pos + 1;
    if (len) {
      memmove(buffer, prev_read_pos, len);
      buffer += len;
    }
    *buffer = 0;
  }
}
