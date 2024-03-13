/**
 * Copyright 2024 Centreon
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

#ifndef CCB_BAM_SERVICE_STATE_HH
#define CCB_BAM_SERVICE_STATE_HH

#include "bbdo/bam_state.pb.h"
#include "com/centreon/broker/bam/service_listener.hh"
#include "com/centreon/broker/neb/acknowledgement.hh"

namespace com::centreon::broker::bam {
struct service_state {
  uint64_t host_id = 0;
  uint64_t service_id = 0;
  State current_state = State::UNKNOWN;
  State last_hard_state = State::UNKNOWN;
  time_t last_check = 0;
  bool state_type = false;
  bool acknowledged = false;
};
}  // namespace com::centreon::broker::bam

namespace fmt {
template <>
struct formatter<com::centreon::broker::bam::service_state> {
  constexpr auto parse(format_parse_context& ctx) -> decltype(ctx.begin()) {
    return ctx.begin();
  }

  // Formats the point p using the parsed format specification (presentation)
  // stored in this formatter.
  template <typename FormatContext>
  auto format(const com::centreon::broker::bam::service_state& state,
              FormatContext& ctx) const -> decltype(ctx.out()) {
    return format_to(ctx.out(),
                     "service state on service ({},{}): current state: {}, "
                     "last hard state: {}, last check: {}, state type: {}, "
                     "acknowledged: {}",
                     state.host_id, state.service_id,
                     State_Name(state.current_state),
                     State_Name(state.last_hard_state), state.last_check,
                     state.state_type, state.acknowledged);
  }
};
}  // namespace fmt

#endif /* !CCB_BAM_SERVICE_STATE_HH */
