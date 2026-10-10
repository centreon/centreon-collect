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

#ifndef CCE_MACROS_GRAB_HH
#define CCE_MACROS_GRAB_HH

#include <fmt/format.h>

#include <string>
#include <string_view>
#include <type_traits>

#include "com/centreon/engine/macros/process.hh"
#include "com/centreon/engine/string.hh"

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
 *  Extract double.
 *
 *  @param[in] t   Host object.
 *  @param[in] mac Unused.
 *
 *  @return Newly allocated string with value as a fixed point string.
 */
template <typename T, double (T::*member)() const, unsigned int precision>
std::string get_double(T& t, nagios_macros* mac) {
  (void)mac;
  return fmt::format("{:.{}f}", (t.*member)(), precision);
}

template <typename T,
          typename V,
          double (V::*member)() const,
          unsigned int precision>
std::string get_double(T& t, nagios_macros* mac) {
  (void)mac;

  V* v{&t};
  return fmt::format("{:.{}f}", (v->*member)(), precision);
}

/**
 *  Extract duration.
 *
 *  @param[in] t   Base object.
 *  @param[in] mac Unused.
 *
 *  @return Duration in a newly allocated string.
 */
template <typename T>
std::string get_duration(T& t, nagios_macros* mac) {
  (void)mac;

  // Get duration.
  time_t now = time(nullptr);
  uint64_t duration = now - t.get_last_state_change();

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
 *  Extract duration in seconds.
 *
 *  @param[in] t   Base object.
 *  @param[in] mac Unused.
 *
 *  @return Duration in second in a newly allocated string.
 */
template <typename T>
std::string get_duration_sec(T& t, nagios_macros* mac) {
  (void)mac;

  // Get duration.
  time_t now = time(nullptr);
  uint64_t duration = now - t.get_last_state_change();
  return fmt::to_string(duration);
}

/**
 *  Copy macro.
 *
 *  @param[in] t   Unused.
 *  @param[in] mac Macro array.
 *
 *  @return Copy of the requested macro.
 */
template <typename T, unsigned int macro_id>
std::string get_macro_copy(T& t, nagios_macros* mac) {
  (void)t;
  return mac->x[macro_id];
}

/**
 *  Get string copy of object member.
 *
 *  @param[in] t   Base object.
 *  @param[in] mac Unused.
 *
 *  @return String copy of object member.
 */
template <typename T, typename U, U (T::*member)() const>
std::string get_member_as_string(T& t, nagios_macros* mac) {
  (void)mac;
  return to_macro_string((t.*member)());
}

/**
 *  Get string copy of object member.
 *
 *  @param[in] t   Base object.
 *  @param[in] mac Unused.
 *
 *  @return String copy of object member.
 */
template <typename T, typename U, typename V, U (V::*member)() const>
std::string get_member_as_string(T& t, nagios_macros* mac) {
  (void)mac;
  V* v{&t};
  return to_macro_string((v->*member)());
}

/**
 *  Recursively process macros.
 *
 *  @param[in] hst Host object.
 *  @param[in] mac Unused.
 *
 *  @return Newly allocated string with macros processed.
 */
template <typename T,
          std::string const& (T::*member)() const,
          unsigned int options>
std::string get_recursive(T& t, nagios_macros* mac) {
  (void)mac;

  // Get copy of string with macros processed.
  std::string buffer;
  process_macros_r(mac, (t.*member)(), buffer, options);
  return buffer;
}
/**
 *  Recursively process macros.
 *
 *  @param[in] hst Host object.
 *  @param[in] mac Unused.
 *
 *  @return Newly allocated string with macros processed.
 */
template <typename T,
          typename V,
          std::string const& (V::*member)() const,
          unsigned int options>
std::string get_recursive(T& t, nagios_macros* mac) {
  (void)mac;

  // Get copy of string with macros processed.
  std::string buffer;
  V* v{&t};
  process_macros_r(mac, (v->*member)(), buffer, options);
  return buffer;
}

/**
 *  Extract state type.
 *
 *  @param[in] t   Base object.
 *  @param[in] mac Unused.
 *
 *  @return Newly allocated state type as a string.
 */
template <typename T>
std::string get_state_type(T& t, nagios_macros* mac) {
  (void)mac;
  return (t.get_state_type() == notifier::hard) ? "HARD" : "SOFT";
}

}  // namespace com::centreon::engine::macros

#endif  // !CCE_MACROS_GRAB_HH
