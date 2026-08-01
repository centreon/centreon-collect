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

#include <gtest/gtest.h>

#include "common/engine_conf/hostescalation_helper.hh"
#include "common/engine_conf/serviceescalation_helper.hh"
#include "common/engine_conf/state.pb.h"
#include "common/notifications/notification_types.hh"

using namespace com::centreon::engine::configuration;
namespace notifications = com::centreon::common::notifications;

/* No option set maps to no flag. */
TEST(EscalationOptionsToFlags, None) {
  EXPECT_EQ(host_escalation_options_to_flags(action_he_none),
            notifications::none);
  EXPECT_EQ(service_escalation_options_to_flags(action_se_none),
            notifications::none);
}

/* Each host option maps to its notification_flag (recovery → up). */
TEST(EscalationOptionsToFlags, HostSingleOptions) {
  EXPECT_EQ(host_escalation_options_to_flags(action_he_down),
            notifications::down);
  EXPECT_EQ(host_escalation_options_to_flags(action_he_unreachable),
            notifications::unreachable);
  EXPECT_EQ(host_escalation_options_to_flags(action_he_recovery),
            notifications::up);
}

/* Each service option maps to its notification_flag (recovery → ok); the proto
 * `pending` state has no notification_flag counterpart. */
TEST(EscalationOptionsToFlags, ServiceSingleOptions) {
  EXPECT_EQ(service_escalation_options_to_flags(action_se_warning),
            notifications::warning);
  EXPECT_EQ(service_escalation_options_to_flags(action_se_unknown),
            notifications::unknown);
  EXPECT_EQ(service_escalation_options_to_flags(action_se_critical),
            notifications::critical);
  EXPECT_EQ(service_escalation_options_to_flags(action_se_recovery),
            notifications::ok);
  EXPECT_EQ(service_escalation_options_to_flags(action_se_pending),
            notifications::none);
}

/* Combined options OR their respective flags together. */
TEST(EscalationOptionsToFlags, Combined) {
  EXPECT_EQ(
      host_escalation_options_to_flags(action_he_down | action_he_recovery),
      notifications::down | notifications::up);
  EXPECT_EQ(service_escalation_options_to_flags(
                action_se_critical | action_se_warning | action_se_recovery),
            notifications::critical | notifications::warning | notifications::ok);
}
