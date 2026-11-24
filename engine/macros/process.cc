/**
 * Copyright 2011-2026 Centreon
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

#include "engine/macros/process.hh"

#include <algorithm>

#include "com/centreon/engine/globals.hh"
#include "engine/macros/resolve.hh"

namespace com::centreon::engine::macros {

/**
 *  Replace the macros of a line by their values (develop #3396, #3408,
 *  #3429):
 *  - "$$" gives a '$';
 *  - a '$' without its closing '$', at the end of the line or not, is kept;
 *  - $X$ is replaced by the value of X. If X cannot be resolved, it is
 *    replaced by nothing in the line itself, but kept as is inside the value
 *    of a custom macro (mac.custom_macro_depth > 0): such a value may hold a
 *    '$' for itself, as in "name in ('MSSQL$SIG','MSSQL$RH')";
 *  - {{$X$}} is replaced by the value of X, by nothing if X cannot be
 *    resolved, wherever it is. Without its closing "$}}", the rest of the line
 *    is kept as is.
 *
 *  @param[in] mac     The macros.
 *  @param[in] input   The line.
 *  @param[in] options STRIP_ILLEGAL_MACRO_CHARS to remove the illegal
 *                     characters from all the values, they are always
 *                     removed from the macros whose descriptor says so.
 *
 *  @return The line with the values of its macros.
 */
std::string process_macros(macro_context& mac,
                           std::string_view input,
                           int options) {
  std::string retval;

  functions_logger->trace("process_macros()");

  if (input.empty())
    return retval;

  macros_logger->trace("**** BEGIN MACRO PROCESSING **** Processing: '{}'",
                       input);

  retval.reserve(input.size());
  const bool strip_illegal = options & STRIP_ILLEGAL_MACRO_CHARS;
  size_t offset = 0;
  while (offset < input.size()) {
    const size_t tag_begin = input.find('$', offset);
    if (tag_begin == std::string_view::npos)
      break;

    // "$$" gives a '$'.
    if (tag_begin + 1 < input.size() && input[tag_begin + 1] == '$') {
      retval.append(input.substr(offset, tag_begin - offset + 1));
      offset = tag_begin + 2;
      continue;
    }

    // {{$X$}} or $X$: the text before the tag is copied.
    const bool braces = tag_begin >= offset + 2 &&
                        input[tag_begin - 1] == '{' &&
                        input[tag_begin - 2] == '{';
    const size_t tag_start = braces ? tag_begin - 2 : tag_begin;
    retval.append(input.substr(offset, tag_start - offset));
    offset = tag_start;

    const size_t tag_end = braces ? input.find("$}}", tag_begin + 1)
                                  : input.find('$', tag_begin + 1);
    // Without its closing, the rest of the line is kept as is.
    if (tag_end == std::string_view::npos)
      break;

    const std::string_view token =
        input.substr(tag_begin + 1, tag_end - tag_begin - 1);
    const std::optional<std::string> token_value =
        value(mac, token, strip_illegal);
    if (token_value) {
      macros_logger->trace("  Processed '{}', To '{}'", token, *token_value);
      retval.append(*token_value);
    } else {
      macros_logger->trace("  '{}' cannot be resolved", token);
      if (!braces && mac.custom_macro_depth > 0)
        retval.append(input.substr(tag_begin, tag_end - tag_begin + 1));
    }
    offset = braces ? tag_end + 3 : tag_end + 1;
  }

  // The text after the last macro.
  if (offset < input.size())
    retval.append(input.substr(offset));

  macros_logger->trace(
      "  Done.  Final output: '{}' **** END MACRO PROCESSING ****", retval);
  return retval;
}

/**
 *  Remove the illegal characters from the value of a macro: the ASCII
 *  control characters and the illegal_output_chars of the configuration.
 *  The non-ASCII characters (Japanese, etc) are kept.
 *
 *  @param[in,out] value The value of the macro.
 */
void strip_illegal_macro_chars(std::string& value) {
  const std::string& illegal = pb_indexed_config.state().illegal_output_chars();
  value.erase(std::remove_if(value.begin(), value.end(),
                             [&illegal](char c) {
                               const unsigned char ch = c;
                               return ch < 32 || ch == 127 ||
                                      illegal.find(c) != std::string::npos;
                             }),
              value.end());
}

/**
 *  Split a command specification as configured, "name!arg1!arg2", into its
 *  arguments. The name is skipped, a backslash escapes the next character
 *  (so that a backslash before a '!' keeps it inside the argument), and at
 *  most MAX_COMMAND_ARGUMENTS arguments are kept.
 *
 *  @param[in] spec The command specification.
 *
 *  @return The arguments, without their escapes.
 */
std::vector<std::string> split_arguments(std::string_view spec) {
  std::vector<std::string> retval;
  size_t pos = spec.find('!');
  while (pos != std::string_view::npos &&
         retval.size() < MAX_COMMAND_ARGUMENTS) {
    std::string& arg = retval.emplace_back();
    bool escaped = false;
    for (++pos; pos < spec.size(); ++pos) {
      const char c = spec[pos];
      if (c == '\\' && !escaped) {
        escaped = true;
        continue;
      }
      if (c == '!' && !escaped)
        break;
      arg.push_back(c);
      escaped = false;
    }
    if (pos >= spec.size())
      pos = std::string_view::npos;
  }
  return retval;
}

}  // namespace com::centreon::engine::macros
