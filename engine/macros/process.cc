/**
 * Copyright 1999-2010 Ethan Galstad
 * Copyright 2011-2013 Merethis
 *
 * This file is part of Centreon Engine.
 *
 * Centreon Engine is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation.
 *
 * Centreon Engine is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Centreon Engine. If not, see
 * <http://www.gnu.org/licenses/>.
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

}  // namespace com::centreon::engine::macros
