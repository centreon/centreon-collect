/**
 * Copyright 2022-2024 Centreon
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

#include "utf8.hh"

/**
 * @brief Find the first byte of str that does not belong to a valid UTF-8
 * sequence (overlong forms, surrogates and code points above U+10FFFF are
 * invalid).
 *
 * @param str The string to check
 *
 * @return An iterator on this byte, or str.end() if str is valid UTF-8.
 */
static std::string_view::const_iterator _first_invalid_utf8(
    const std::string_view& str) noexcept {
  std::string_view::const_iterator it;
  for (it = str.begin(); it < str.end();) {
    uint32_t val = (*it & 0xff);
    if ((val & 0x80) == 0) {
      ++it;
      continue;
    }
    if (it + 1 >= str.end()) {
      break;
    }
    val = (val << 8) | (*(it + 1) & 0xff);
    if ((val & 0xe0c0) == 0xc080) {
      val &= 0x1e00;
      if (val == 0)
        break;
      it += 2;
      continue;
    }

    if (it + 2 >= str.end()) {
      break;
    }
    val = (val << 8) | (*(it + 2) & 0xff);
    if ((val & 0xf0c0c0) == 0xe08080) {
      val &= 0xf2000;
      if (val == 0 || val == 0xd2000)
        break;
      it += 3;
      continue;
    }

    if (it + 3 >= str.end()) {
      break;
    }
    val = (val << 8) | (*(it + 3) & 0xff);
    if ((val & 0xf8c0c0c0) == 0xF0808080) {
      val &= 0x7300000;
      if (val == 0 || val > 0x4000000)
        break;
      it += 4;
      continue;
    }
    break;
  }
  return it;
}

/**
 * @brief Checks if the string given as parameter is a real UTF-8 string.
 * If it is not, it tries to convert it to UTF-8. Encodings correctly changed
 * are ISO-8859-15 and CP-1252.
 *
 * @param str The string to check
 *
 * @return The string itself or a new string converted to UTF-8. The output
 * string should always be an UTF-8 string.
 */
std::string com::centreon::common::check_string_utf8(
    const std::string_view& str) noexcept {
  std::string_view::const_iterator it = _first_invalid_utf8(str);

  if (it == str.end())
    return std::string(str);

  /* Not an UTF-8 string */
  bool is_cp1252 = true, is_iso8859 = true;
  auto itt = it;

  auto iso8859_to_utf8 = [&str, &it]() -> std::string {
    /* Strings are both cp1252 and iso8859-15 */
    std::string out;
    std::size_t d = it - str.begin();
    out.reserve(d + 2 * (str.size() - d));
    out = str.substr(0, d);
    while (it != str.end()) {
      uint8_t c = static_cast<uint8_t>(*it);
      if (c < 128)
        out.push_back(c);
      else if (c <= 160)
        out.push_back('_');
      else {
        switch (c) {
          case 0xa4:
            out.append("€");
            break;
          case 0xa6:
            out.append("Š");
            break;
          case 0xa8:
            out.append("š");
            break;
          case 0xb4:
            out.append("Ž");
            break;
          case 0xb8:
            out.append("ž");
            break;
          case 0xbc:
            out.append("Œ");
            break;
          case 0xbd:
            out.append("œ");
            break;
          case 0xbe:
            out.append("Ÿ");
            break;
          default:
            out.push_back(0xc0 | c >> 6);
            out.push_back((c & 0x3f) | 0x80);
            break;
        }
      }
      ++it;
    }
    return out;
  };
  do {
    uint8_t c = *itt;
    /* not ISO-8859-15 */
    if (c > 126 && c < 160)
      is_iso8859 = false;
    /* not cp1252 */
    if (c & 128)
      if (c == 129 || c == 141 || c == 143 || c == 144 || c == 157)
        is_cp1252 = false;
    if (!is_cp1252)
      return iso8859_to_utf8();
    else if (!is_iso8859) {
      std::string out;
      std::size_t d = it - str.begin();
      out.reserve(d + 3 * (str.size() - d));
      out = str.substr(0, d);
      while (it != str.end()) {
        c = *it;
        if (c < 128)
          out.push_back(c);
        else {
          switch (c) {
            case 128:
              out.append("€");
              break;
            case 129:
            case 141:
            case 143:
            case 144:
            case 157:
              out.append("_");
              break;
            case 130:
              out.append("‚");
              break;
            case 131:
              out.append("ƒ");
              break;
            case 132:
              out.append("„");
              break;
            case 133:
              out.append("…");
              break;
            case 134:
              out.append("†");
              break;
            case 135:
              out.append("‡");
              break;
            case 136:
              out.append("ˆ");
              break;
            case 137:
              out.append("‰");
              break;
            case 138:
              out.append("Š");
              break;
            case 139:
              out.append("‹");
              break;
            case 140:
              out.append("Œ");
              break;
            case 142:
              out.append("Ž");
              break;
            case 145:
              out.append("‘");
              break;
            case 146:
              out.append("’");
              break;
            case 147:
              out.append("“");
              break;
            case 148:
              out.append("”");
              break;
            case 149:
              out.append("•");
              break;
            case 150:
              out.append("–");
              break;
            case 151:
              out.append("—");
              break;
            case 152:
              out.append("˜");
              break;
            case 153:
              out.append("™");
              break;
            case 154:
              out.append("š");
              break;
            case 155:
              out.append("›");
              break;
            case 156:
              out.append("œ");
              break;
            case 158:
              out.append("ž");
              break;
            case 159:
              out.append("Ÿ");
              break;
            default:
              out.push_back(0xc0 | c >> 6);
              out.push_back((c & 0x3f) | 0x80);
              break;
          }
        }
        ++it;
      }
      return out;
    }
    ++itt;
  } while (itt != str.end());
  assert(is_cp1252 == is_iso8859);
  return iso8859_to_utf8();
}

/**
 * @brief This function adjusts the given integer s so that the str string may
 * be cut at this length and still be a UTF-8 string (we don't want to cut it
 * in a middle of a character).
 *
 * This function assumes the string to be UTF-8 encoded.
 *
 * @param str A string to truncate.
 * @param s The desired size, maybe the resulting string will contain less
 * characters.
 *
 * @return The newly computed size.
 */
size_t com::centreon::common::adjust_size_utf8(const std::string& str,
                                               size_t s) {
  if (s >= str.size())
    return str.size();
  if (s == 0)
    return s;
  else {
    while ((str[s] & 0xc0) == 0x80)
      s--;
    return s;
  }
}

/**
 * @brief Checks if str is a valid UTF-8 string.
 *
 * @param str The string to check
 *
 * @return true if str is valid UTF-8.
 */
bool com::centreon::common::is_utf8(const std::string_view& str) noexcept {
  return _first_invalid_utf8(str) == str.end();
}

/**
 * @brief Decodes the UTF-8 sequence starting at str[*pos].
 *
 * @param str The UTF-8 string
 * @param pos in: index of the sequence, out: index of the next one. An
 * invalid byte is skipped alone.
 *
 * @return The code point, or U+FFFD if the sequence is invalid.
 */
static char32_t _decode_utf8(const std::string_view& str, size_t* pos) {
  unsigned char c = str[*pos];
  if (c < 0x80) {
    ++*pos;
    return c;
  }
  size_t len;
  char32_t code_point, min_code_point;
  if ((c & 0xe0) == 0xc0) {
    len = 2;
    code_point = c & 0x1f;
    min_code_point = 0x80;
  } else if ((c & 0xf0) == 0xe0) {
    len = 3;
    code_point = c & 0x0f;
    min_code_point = 0x800;
  } else if ((c & 0xf8) == 0xf0) {
    len = 4;
    code_point = c & 0x07;
    min_code_point = 0x10000;
  } else {
    ++*pos;
    return 0xfffd;
  }
  if (*pos + len > str.size()) {
    ++*pos;
    return 0xfffd;
  }
  for (size_t i = 1; i < len; ++i) {
    unsigned char cont = str[*pos + i];
    if ((cont & 0xc0) != 0x80) {
      ++*pos;
      return 0xfffd;
    }
    code_point = (code_point << 6) | (cont & 0x3f);
  }
  if (code_point < min_code_point || code_point > 0x10ffff ||
      (code_point >= 0xd800 && code_point <= 0xdfff)) {
    ++*pos;
    return 0xfffd;
  }
  *pos += len;
  return code_point;
}

/**
 * @brief Appends the UTF-8 encoding of a code point to out. Surrogates and
 * values above U+10FFFF are replaced by U+FFFD.
 *
 * @param code_point The code point to encode
 * @param out The string to append to
 */
static void _append_utf8(char32_t code_point, std::string* out) {
  if (code_point > 0x10ffff || (code_point >= 0xd800 && code_point <= 0xdfff)) {
    code_point = 0xfffd;
  }
  if (code_point < 0x80) {
    out->push_back(static_cast<char>(code_point));
  } else if (code_point < 0x800) {
    out->push_back(static_cast<char>(0xc0 | (code_point >> 6)));
    out->push_back(static_cast<char>(0x80 | (code_point & 0x3f)));
  } else if (code_point < 0x10000) {
    out->push_back(static_cast<char>(0xe0 | (code_point >> 12)));
    out->push_back(static_cast<char>(0x80 | ((code_point >> 6) & 0x3f)));
    out->push_back(static_cast<char>(0x80 | (code_point & 0x3f)));
  } else {
    out->push_back(static_cast<char>(0xf0 | (code_point >> 18)));
    out->push_back(static_cast<char>(0x80 | ((code_point >> 12) & 0x3f)));
    out->push_back(static_cast<char>(0x80 | ((code_point >> 6) & 0x3f)));
    out->push_back(static_cast<char>(0x80 | (code_point & 0x3f)));
  }
}

/**
 * @brief Converts an UTF-8 string to a wide string (UTF-16 on Windows, UTF-32
 * elsewhere). Invalid UTF-8 bytes are replaced by U+FFFD.
 *
 * @param str The UTF-8 string
 *
 * @return The wide string.
 */
std::wstring com::centreon::common::utf8_to_wstring(
    const std::string_view& str) {
  std::wstring ret;
  ret.reserve(str.size());
  for (size_t pos = 0; pos < str.size();) {
    char32_t code_point = _decode_utf8(str, &pos);
    if constexpr (sizeof(wchar_t) == 2) {
      if (code_point >= 0x10000) {
        code_point -= 0x10000;
        ret.push_back(static_cast<wchar_t>(0xd800 + (code_point >> 10)));
        ret.push_back(static_cast<wchar_t>(0xdc00 + (code_point & 0x3ff)));
        continue;
      }
    }
    ret.push_back(static_cast<wchar_t>(code_point));
  }
  return ret;
}

/**
 * @brief Converts a wide string (UTF-16 on Windows, UTF-32 elsewhere) to an
 * UTF-8 string. Unpaired surrogates are replaced by U+FFFD.
 *
 * @param str The wide string
 *
 * @return The UTF-8 string.
 */
std::string com::centreon::common::wstring_to_utf8(
    const std::wstring_view& str) {
  std::string ret;
  ret.reserve(str.size());
  for (size_t pos = 0; pos < str.size(); ++pos) {
    char32_t code_point = static_cast<char32_t>(str[pos]);
    if constexpr (sizeof(wchar_t) == 2) {
      if (code_point >= 0xd800 && code_point <= 0xdbff &&
          pos + 1 < str.size()) {
        char32_t low = static_cast<char32_t>(str[pos + 1]);
        if (low >= 0xdc00 && low <= 0xdfff) {
          code_point = 0x10000 + ((code_point - 0xd800) << 10) + (low - 0xdc00);
          ++pos;
        }
      }
    }
    _append_utf8(code_point, &ret);
  }
  return ret;
}
