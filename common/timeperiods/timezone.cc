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
#include "common/timeperiods/timezone.hh"

#include <string_view>

#include "absl/time/clock.h"

namespace com::centreon::common::timeperiods {

/**
 * @brief Convert an engine timezone directive into an immutable absl::TimeZone.
 *
 * The returned zone is meant to be passed as the per-call timezone parameter of
 * the timeperiods library, replacing the former process-global setenv/tzset
 * approach. The configuration stores timezones in the TZ environment form, usually with a
 * leading ':' (e.g. ":Europe/Paris"); absl::LoadTimeZone wants the bare IANA
 * name. An empty directive (the common case) means "use the daemon's local
 * timezone", and an unparsable name falls back to it as well.
 *
 * @param name The timezone directive (possibly empty, possibly ":"-prefixed).
 *
 * @return The matching timezone, or the local timezone as a fallback.
 */
absl::TimeZone string_to_timezone(const std::string& name) {
  if (name.empty())
    return absl::LocalTimeZone();

  std::string_view bare = name;
  if (bare.front() == ':')
    bare.remove_prefix(1);
  absl::TimeZone tz;
  if (absl::LoadTimeZone(bare, &tz))
    return tz;
  return absl::LocalTimeZone();
}

}  // namespace com::centreon::common::timeperiods
