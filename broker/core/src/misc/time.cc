/**
* Copyright 2021 Centreon
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

#include "com/centreon/broker/misc/time.hh"
#include "com/centreon/exceptions/msg_fmt.hh"

using namespace com::centreon::broker;
using namespace com::centreon::exceptions;

/**
 * @brief Compute the start of day as a time_t value.
 *
 * @param when A time_t.
 *
 * @return A time_t.
 */
std::time_t misc::start_of_day(time_t when) {
  struct tm tmv;
  if (!localtime_r(&when, &tmv))
    throw msg_fmt("misc::time: Cannot compute the start of the date {}", when);
  tmv.tm_sec = tmv.tm_min = tmv.tm_hour = 0;
  return mktime(&tmv);
}
