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

#include "common/notifications/contact_viability.hh"

#include <gtest/gtest.h>

#include "common/log_v2/log_v2.hh"

using namespace com::centreon::common::notifications;

/* should_notify_contact logs its rejection reasons on the NOTIFICATIONS
 * channel, so log_v2 must be loaded around these tests. */
class ContactViability : public ::testing::Test {
 public:
  void SetUp() override {
    com::centreon::common::log_v2::log_v2::load("ut_common");
  }
  void TearDown() override { com::centreon::common::log_v2::log_v2::unload(); }
};

namespace {
/* A contact accepting everything on both host and service, in period. */
contact all_on() {
  contact c;
  c.name = "c";
  c.host_notifications_enabled = true;
  c.service_notifications_enabled = true;
  c.host_notification_options = up | down | unreachable | flappingstart |
                                flappingstop | flappingdisabled | downtime;
  c.service_notification_options = ok | warning | critical | unknown |
                                   flappingstart | flappingstop |
                                   flappingdisabled | downtime;
  return c;
}

/* The normal notification a recovery is restricted to: it told "c", the contact
 * all_on() builds. A nullptr stands for a resource that never notified. */
notification told_c() {
  notification n;
  n.type = reason_normal;
  n.add_contacts({"c"});
  return n;
}
}  // namespace

/* The host/service notification-enable flag gates everything. */
TEST_F(ContactViability, EnableFlag) {
  contact c = all_on();

  c.service_notifications_enabled = false;
  EXPECT_FALSE(should_notify_contact(c, /*is_host=*/false, cat_acknowledgement,
                                     reason_acknowledgement, 0,
                                     /*in_period=*/true, nullptr));
  /* The host flag is independent: a host notification still goes through. */
  EXPECT_TRUE(should_notify_contact(c, /*is_host=*/true, cat_acknowledgement,
                                    reason_acknowledgement, 0, true, nullptr));

  c = all_on();
  c.host_notifications_enabled = false;
  EXPECT_FALSE(should_notify_contact(c, true, cat_acknowledgement,
                                     reason_acknowledgement, 0, true, nullptr));
}

/* An out-of-period contact is never notified. */
TEST_F(ContactViability, OutOfPeriod) {
  contact c = all_on();
  EXPECT_FALSE(should_notify_contact(c, false, cat_acknowledgement,
                                     reason_acknowledgement, 0,
                                     /*in_period=*/false, nullptr));
}

/* Normal notification: the current state must map to a bit set in the mask. */
TEST_F(ContactViability, NormalStateToBitService) {
  contact c = all_on();
  /* Accept only CRITICAL (service state 2). */
  c.service_notification_options = critical;

  EXPECT_TRUE(should_notify_contact(c, false, cat_normal, reason_normal,
                                    /*state=*/2, true, nullptr));
  /* WARNING (state 1) is not in the mask. */
  EXPECT_FALSE(should_notify_contact(c, false, cat_normal, reason_normal, 1,
                                     true, nullptr));
  /* OK (state 0) is not in the mask either. */
  EXPECT_FALSE(should_notify_contact(c, false, cat_normal, reason_normal, 0,
                                     true, nullptr));
}

TEST_F(ContactViability, NormalStateToBitHost) {
  contact c = all_on();
  c.host_notification_options = down;  // accept only DOWN (host state 1)

  EXPECT_TRUE(should_notify_contact(c, true, cat_normal, reason_normal,
                                    /*state=*/1, true, nullptr));
  EXPECT_FALSE(should_notify_contact(c, true, cat_normal, reason_normal,
                                     /*unreachable=*/2, true, nullptr));
}

/* Recovery: needs the ok/up bit AND the contact to have been notified of the
 * problem. */
TEST_F(ContactViability, Recovery) {
  contact c = all_on();
  const notification told = told_c();

  /* accepts recovery + was told about the problem -> true */
  EXPECT_TRUE(should_notify_contact(c, false, cat_recovery, reason_recovery, 0,
                                    true, &told));
  /* the resource never notified anybody -> false */
  EXPECT_FALSE(should_notify_contact(c, false, cat_recovery, reason_recovery, 0,
                                     true, nullptr));
  /* it notified, but somebody else -> false */
  contact other = all_on();
  other.name = "other";
  EXPECT_FALSE(should_notify_contact(other, false, cat_recovery,
                                     reason_recovery, 0, true, &told));

  /* does not accept the ok/up bit -> false even when told about the problem */
  c.service_notification_options = critical;  // no ok bit
  EXPECT_FALSE(should_notify_contact(c, false, cat_recovery, reason_recovery, 0,
                                     true, &told));
}

/* Flapping: the flag depends on the reason. */
TEST_F(ContactViability, Flapping) {
  contact c = all_on();
  c.service_notification_options = flappingstart;  // only accept start

  EXPECT_TRUE(should_notify_contact(
      c, false, cat_flapping, reason_flappingstart, 0, true, nullptr));
  EXPECT_FALSE(should_notify_contact(
      c, false, cat_flapping, reason_flappingstop, 0, true, nullptr));
}

TEST_F(ContactViability, Downtime) {
  contact c = all_on();
  EXPECT_TRUE(should_notify_contact(
      c, false, cat_downtime, reason_downtimestart, 0, true, nullptr));
  c.service_notification_options = critical;  // no downtime bit
  EXPECT_FALSE(should_notify_contact(
      c, false, cat_downtime, reason_downtimestart, 0, true, nullptr));
}

/* Acknowledgement and custom are unconditional (once enabled and in period). */
TEST_F(ContactViability, AcknowledgementAndCustomUnconditional) {
  contact c = all_on();
  c.service_notification_options = none;  // even with an empty mask

  EXPECT_TRUE(should_notify_contact(c, false, cat_acknowledgement,
                                    reason_acknowledgement, 0, true, nullptr));
  EXPECT_TRUE(should_notify_contact(c, false, cat_custom, reason_custom, 0,
                                    true, nullptr));
}

/* Only a recovery consults the notification history. That invariant is what
 * lets the callers hand the notification over unconditionally instead of
 * repeating the category test on each side, so it is worth pinning: a contact
 * left out of the problem audience is still notified by every other category. */
TEST_F(ContactViability, ProblemAudienceOnlyRestrictsRecovery) {
  contact c = all_on();
  notification told_somebody_else;
  told_somebody_else.type = reason_normal;
  told_somebody_else.add_contacts({"other"});

  EXPECT_FALSE(should_notify_contact(c, false, cat_recovery, reason_recovery, 0,
                                     true, &told_somebody_else));

  EXPECT_TRUE(should_notify_contact(c, false, cat_normal, reason_normal,
                                    2 /* critical */, true,
                                    &told_somebody_else));
  EXPECT_TRUE(should_notify_contact(c, false, cat_acknowledgement,
                                    reason_acknowledgement, 0, true,
                                    &told_somebody_else));
  EXPECT_TRUE(should_notify_contact(c, false, cat_flapping,
                                    reason_flappingstart, 0, true,
                                    &told_somebody_else));
  EXPECT_TRUE(should_notify_contact(c, false, cat_downtime,
                                    reason_downtimestart, 0, true,
                                    &told_somebody_else));
  EXPECT_TRUE(should_notify_contact(c, false, cat_custom, reason_custom, 0,
                                    true, &told_somebody_else));
}
