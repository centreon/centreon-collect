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
 *  Replace the macros of a line by their values. "$$" gives a '$', a '$'
 *  without its closing '$' is dropped.
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

  const bool strip_illegal = options & STRIP_ILLEGAL_MACRO_CHARS;
  for (auto it = input.begin(), end = input.end(); it != end; ++it) {
    if (*it != '$') {
      retval += *it;
      continue;
    }
    // A last '$' is dropped.
    if (std::next(it) == end)
      continue;
    // "$$" gives a '$'.
    if (*std::next(it) == '$') {
      retval += '$';
      ++it;
      continue;
    }
    const size_t where = it - input.begin();
    const size_t pos = input.find('$', where + 1);
    // A '$' without its closing '$' is dropped.
    if (pos == std::string_view::npos)
      continue;

    const std::string_view token = input.substr(where + 1, pos - where - 1);
    const std::string token_value = value(mac, token, strip_illegal);
    macros_logger->trace("  Processed '{}', To '{}'", token, token_value);
    retval.append(token_value);
    it += pos - where;
  }

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
