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

#include "common/notifications/escalation.hh"

#include <gtest/gtest.h>

using namespace com::centreon::common::notifications;

namespace {
/* A service escalation on critical, notifications 3..0 (unbounded), interval
 * 300, in period, notifying contactgroup "cg". */
escalation svc_esc() {
  escalation e;
  e.first_notification = 3;
  e.last_notification = 0;
  e.notification_interval = 300;
  e.escalate_on = critical;
  e.in_period = true;
  e.contactgroups = {"cg"};
  return e;
}
}  // namespace

/* No escalation attached: not escalated, no group. */
TEST(EvaluateEscalations, Empty) {
  auto ev = evaluate_escalations({}, service_notification, 2, 5);
  EXPECT_FALSE(ev.escalated);
  EXPECT_TRUE(ev.contactgroups.empty());
}

/* A single viable escalation: escalated, its interval and group are returned.
 */
TEST(EvaluateEscalations, SingleViable) {
  std::vector<escalation> esc{svc_esc()};
  auto ev = evaluate_escalations(esc, service_notification, /*critical=*/2, 5);
  EXPECT_TRUE(ev.escalated);
  EXPECT_EQ(ev.notification_interval, 300u);
  ASSERT_EQ(ev.contactgroups.size(), 1u);
  EXPECT_EQ(*ev.contactgroups.begin(), "cg");
}

/* escalate_on must cover the current state. */
TEST(EvaluateEscalations, StateFilter) {
  std::vector<escalation> esc{svc_esc()};  // critical only
  /* warning (state 1) is not covered. */
  EXPECT_FALSE(evaluate_escalations(esc, service_notification, 1, 5).escalated);
  /* critical (state 2) is covered. */
  EXPECT_TRUE(evaluate_escalations(esc, service_notification, 2, 5).escalated);
}

/* [first, last] must cover the notification number; last == 0 is unbounded. */
TEST(EvaluateEscalations, NotificationNumberRange) {
  std::vector<escalation> esc{svc_esc()};  // first=3, last=0
  EXPECT_FALSE(evaluate_escalations(esc, service_notification, 2, 2).escalated);
  EXPECT_TRUE(evaluate_escalations(esc, service_notification, 2, 3).escalated);
  EXPECT_TRUE(evaluate_escalations(esc, service_notification, 2, 99).escalated);

  esc[0].last_notification = 5;  // now bounded
  EXPECT_TRUE(evaluate_escalations(esc, service_notification, 2, 5).escalated);
  EXPECT_FALSE(evaluate_escalations(esc, service_notification, 2, 6).escalated);
}

/* An escalation outside its period is skipped. */
TEST(EvaluateEscalations, OutOfPeriod) {
  std::vector<escalation> esc{svc_esc()};
  esc[0].in_period = false;
  EXPECT_FALSE(evaluate_escalations(esc, service_notification, 2, 5).escalated);
}

/* Among several viable escalations, the smallest interval wins and the groups
 * are unioned. */
TEST(EvaluateEscalations, SmallestIntervalAndUnion) {
  escalation a = svc_esc();
  a.notification_interval = 300;
  a.contactgroups = {"cg1"};
  escalation b = svc_esc();
  b.notification_interval = 120;
  b.contactgroups = {"cg2"};
  escalation c = svc_esc();
  c.notification_interval = 600;
  c.in_period = false;  // not viable, must not lower the interval
  c.contactgroups = {"cg3"};

  std::vector<escalation> esc{a, b, c};
  auto ev = evaluate_escalations(esc, service_notification, 2, 5);
  EXPECT_TRUE(ev.escalated);
  EXPECT_EQ(ev.notification_interval, 120u);
  EXPECT_EQ(ev.contactgroups, (absl::btree_set<std::string>{"cg1", "cg2"}));
}

/* Host escalations use the host state-to-flag mapping. */
TEST(EvaluateEscalations, HostStateMapping) {
  escalation e = svc_esc();
  e.escalate_on = down;  // host down
  std::vector<escalation> esc{e};
  EXPECT_TRUE(
      evaluate_escalations(esc, host_notification, /*down=*/1, 5).escalated);
  EXPECT_FALSE(
      evaluate_escalations(esc, host_notification, /*up=*/0, 5).escalated);
}

/* An out-of-range state yields no escalation rather than reading out of the
 * flag array. */
TEST(EvaluateEscalations, StateOutOfRange) {
  std::vector<escalation> esc{svc_esc()};
  EXPECT_FALSE(evaluate_escalations(esc, service_notification, 4, 5).escalated);
  EXPECT_FALSE(evaluate_escalations(esc, host_notification, 3, 5).escalated);
  EXPECT_FALSE(
      evaluate_escalations(esc, service_notification, -1, 5).escalated);
}
