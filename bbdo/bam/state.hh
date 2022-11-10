/*
** Copyright 2021 Centreon
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

#ifndef CCB_BAM_STATE_HH
#define CCB_BAM_STATE_HH

#include "bbdo/bam.pb.h"
#include "com/centreon/broker/namespace.hh"

CCB_BEGIN()

namespace bam {
enum state {
  state_ok = 0,
  state_warning = 1,
  state_critical = 2,
  state_unknown = 3,
};

constexpr std::array<const char*, 4> state_str{"OK", "WARNING", "CRITICAL",
                                               "UNKNOWN"};

inline com::centreon::broker::State bam_state_to_pb(
    com::centreon::broker::bam::state state) {
  switch (state) {
    case state_ok:
      return com::centreon::broker::State::OK;
    case state_warning:
      return com::centreon::broker::State::WARNING;
    case state_critical:
      return com::centreon::broker::State::CRITICAL;
    default:
      return com::centreon::broker::State::UNKNOWN;
  }
}

}  // namespace bam

CCB_END()

#endif  // !CCB_BAM_STATE_HH
