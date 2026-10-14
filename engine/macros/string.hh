/**
 * Copyright 1999-2010 Ethan Galstad
 * Copyright 2011-2013 Merethis
 * Copyright 2014-2026 Centreon
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

#ifndef CCE_MACROS_STRING_HH
#define CCE_MACROS_STRING_HH

#include <fmt/format.h>

#include <string>
#include <string_view>
#include <type_traits>

#include "com/centreon/engine/checkable.hh"
#include "engine/macros/process.hh"

namespace com::centreon::engine::macros {

/**
 *  Convert a value to the string given by a macro. Booleans are written 0
 *  or 1 and enums as their numeric value, as std::ostream did.
 *
 *  @param[in] value The value to convert.
 *
 *  @return The value as a string.
 */
template <typename U>
std::string to_macro_string(const U& value) {
  if constexpr (std::is_same_v<U, bool>)
    return value ? "1" : "0";
  else if constexpr (std::is_enum_v<U>)
    return fmt::to_string(static_cast<std::underlying_type_t<U>>(value));
  else if constexpr (std::is_convertible_v<const U&, std::string_view>)
    return std::string(value);
  else
    return fmt::to_string(value);
}

/**
 *  Format a double as a fixed point number.
 *
 *  @param[in] value     The value to format.
 *  @param[in] precision The number of decimals.
 *
 *  @return The formatted value.
 */
inline std::string fixed_string(double value, int precision) {
  return fmt::format("{:.{}f}", value, precision);
}

/**
 *  The time elapsed since the last state change, as "1d 2h 3m 4s".
 *
 *  @param[in] chk The host or the service.
 *
 *  @return The duration.
 */
inline std::string duration_string(const checkable& chk) {
  uint64_t duration = time(nullptr) - chk.get_last_state_change();

  // Break down duration.
  uint64_t days = duration / (24 * 60 * 60);
  duration %= (24 * 60 * 60);
  uint64_t hours = duration / (60 * 60);
  duration %= (60 * 60);
  uint64_t minutes = duration / 60;
  duration %= 60;

  return fmt::format("{}d {}h {}m {}s", days, hours, minutes, duration);
}

/**
 *  The time elapsed since the last state change, in seconds.
 *
 *  @param[in] chk The host or the service.
 *
 *  @return The duration.
 */
inline std::string duration_sec_string(const checkable& chk) {
  uint64_t duration = time(nullptr) - chk.get_last_state_change();
  return fmt::to_string(duration);
}

/**
 *  The state type of a host or a service.
 *
 *  @param[in] chk The host or the service.
 *
 *  @return "HARD" or "SOFT".
 */
inline std::string state_type_string(const checkable& chk) {
  return chk.get_state_type() == checkable::hard ? "HARD" : "SOFT";
}

/**
 *  The check type of a host or a service.
 *
 *  @param[in] chk The host or the service.
 *
 *  @return "PASSIVE" or "ACTIVE".
 */
inline std::string check_type_string(const checkable& chk) {
  return chk.get_check_type() == checkable::check_passive ? "PASSIVE"
                                                          : "ACTIVE";
}

/**
 *  Process the macros contained in a value (notes, URLs).
 *
 *  @param[in] mac   The macros.
 *  @param[in] value The value to process.
 *
 *  @return The value with its macros processed.
 */
inline std::string recursive_string(macro_context* mac,
                                    const std::string& value) {
  return process_macros(*mac, value, 0);
}

}  // namespace com::centreon::engine::macros

#endif  // !CCE_MACROS_STRING_HH
