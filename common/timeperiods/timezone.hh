/**
 * Copyright 2026 Centreon
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
#ifndef CCC_TIMEPERIODS_TIMEZONE_HH
#define CCC_TIMEPERIODS_TIMEZONE_HH

#include <string>

#include "absl/time/time.h"

namespace com::centreon::common::timeperiods {

absl::TimeZone string_to_timezone(const std::string& name);

}  // namespace com::centreon::common::timeperiods

#endif  // !CCC_TIMEPERIODS_TIMEZONE_HH
