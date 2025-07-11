/**
 * Copyright 2025 Centreon
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

#ifndef CENTREON_AGENT_WINDOWS_UTIL_HH
#define CENTREON_AGENT_WINDOWS_UTIL_HH

namespace com::centreon::agent {
std::string get_last_error_as_string();

std::string lpwcstr_to_acp(LPCWSTR lpwstr);

std::chrono::file_clock::time_point convert_filetime_to_tp(uint64_t file_time);

inline std::chrono::file_clock::time_point convert_filetime_to_tp(
    FILETIME file_time) {
  ULARGE_INTEGER uli = {.LowPart = file_time.dwLowDateTime,
                        .HighPart = file_time.dwHighDateTime};
  return convert_filetime_to_tp(uli.QuadPart);
}

inline std::chrono::file_clock::duration convert_filetime_to_duration(
    FILETIME file_time) {
  ULARGE_INTEGER uli = {.LowPart = file_time.dwLowDateTime,
                        .HighPart = file_time.dwHighDateTime};
  return std::chrono::file_clock::duration(uli.QuadPart);
}

void com_init();

}  // namespace com::centreon::agent

#endif
