/*
** Copyright 2023 Centreon
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
**
** For more information : contact@centreon.com
*/

#ifndef CCCM_TIME_HH
#define CCCM_TIME_HH

#include "namespace.hh"

CCCM_BEGIN()

/**
 * @brief converter from google TimeStamp to
 * std::chrono::system_clock::time_point
 *
 * @param google_ts
 * @return std::chrono::system_clock::time_point
 */
inline std::chrono::system_clock::time_point google_ts_to_time_point(
    const google::protobuf::Timestamp& google_ts) {
  return std::chrono::time_point_cast<std::chrono::system_clock::duration>(
      std::chrono::time_point<std::chrono::system_clock,
                              std::chrono::nanoseconds>(
          std::chrono::nanoseconds(google_ts.seconds() * 1000000000 +
                                   google_ts.nanos())));
}

/**
 * @brief converter from std::chrono::system_clock::time_point to google
 * TimeStamp
 *
 * @param tp IN time_point to convert
 * @param google_ts OUT google TimeStamp
 */
inline void time_point_to_google_ts(
    const std::chrono::system_clock::time_point& tp,
    google::protobuf::Timestamp& google_ts) {
  std::chrono::system_clock::duration from_epoch = tp.time_since_epoch();
  google_ts.set_seconds(
      std::chrono::duration_cast<std::chrono::seconds>(from_epoch).count());
  google_ts.set_nanos(
      std::chrono::duration_cast<std::chrono::nanoseconds>(from_epoch).count() %
      1000000000);
}

CCCM_END()

#endif
