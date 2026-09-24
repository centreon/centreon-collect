/**
 * Copyright 2026 Centreon
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

#ifndef CCC_TIME_COMPAT_HH
#define CCC_TIME_COMPAT_HH

#ifdef _WIN32
#include <ctime>

// MSVC does not provide the POSIX reentrant time helpers localtime_r/gmtime_r;
// it ships localtime_s/gmtime_s instead, which take their arguments in the
// opposite order and return an errno_t (0 on success). These inline shims keep
// the POSIX signature (returning the struct tm* on success, nullptr on error)
// so shared time code compiles unchanged on Windows.
inline struct tm* localtime_r(const time_t* timep, struct tm* result) {
  return ::localtime_s(result, timep) == 0 ? result : nullptr;
}

inline struct tm* gmtime_r(const time_t* timep, struct tm* result) {
  return ::gmtime_s(result, timep) == 0 ? result : nullptr;
}
#endif  // _WIN32

#endif  // CCC_TIME_COMPAT_HH
