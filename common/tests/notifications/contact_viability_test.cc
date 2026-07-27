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

using namespace com::centreon::common::notifications;

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
}  // namespace

/* The host/service notification-enable flag gates everything. */
TEST(ContactViability, EnableFlag) {
  contact c = all_on();

  c.service_notifications_enabled = false;
  EXPECT_FALSE(should_notify_contact(c, /*is_host=*/false, cat_acknowledgement,
                                     reason_acknowledgement, 0,
                                     /*in_period=*/true, false));
  /* The host flag is independent: a host notification still goes through. */
  EXPECT_TRUE(should_notify_contact(c, /*is_host=*/true, cat_acknowledgement,
                                    reason_acknowledgement, 0, true, false));

  c = all_on();
  c.host_notifications_enabled = false;
  EXPECT_FALSE(should_notify_contact(c, true, cat_acknowledgement,
                                     reason_acknowledgement, 0, true, false));
}

/* An out-of-period contact is never notified. */
TEST(ContactViability, OutOfPeriod) {
  contact c = all_on();
  EXPECT_FALSE(should_notify_contact(c, false, cat_acknowledgement,
                                     reason_acknowledgement, 0,
                                     /*in_period=*/false, false));
}

/* Normal notification: the current state must map to a bit set in the mask. */
TEST(ContactViability, NormalStateToBitService) {
  contact c = all_on();
  /* Accept only CRITICAL (service state 2). */
  c.service_notification_options = critical;

  EXPECT_TRUE(should_notify_contact(c, false, cat_normal, reason_normal,
                                    /*state=*/2, true, false));
  /* WARNING (state 1) is not in the mask. */
  EXPECT_FALSE(should_notify_contact(c, false, cat_normal, reason_normal, 1,
                                     true, false));
  /* OK (state 0) is not in the mask either. */
  EXPECT_FALSE(should_notify_contact(c, false, cat_normal, reason_normal, 0,
                                     true, false));
}

TEST(ContactViability, NormalStateToBitHost) {
  contact c = all_on();
  c.host_notification_options = down;  // accept only DOWN (host state 1)

  EXPECT_TRUE(should_notify_contact(c, true, cat_normal, reason_normal,
                                    /*state=*/1, true, false));
  EXPECT_FALSE(should_notify_contact(c, true, cat_normal, reason_normal,
                                     /*unreachable=*/2, true, false));
}

/* Recovery: needs the ok/up bit AND the contact to have been notified of the
 * problem. */
TEST(ContactViability, Recovery) {
  contact c = all_on();

  /* accepts recovery + already notified -> true */
  EXPECT_TRUE(should_notify_contact(c, false, cat_recovery, reason_recovery, 0,
                                    true, /*already_notified=*/true));
  /* accepts recovery but was NOT notified of the problem -> false */
  EXPECT_FALSE(should_notify_contact(c, false, cat_recovery, reason_recovery, 0,
                                     true, /*already_notified=*/false));

  /* does not accept the ok/up bit -> false even if already notified */
  c.service_notification_options = critical;  // no ok bit
  EXPECT_FALSE(should_notify_contact(c, false, cat_recovery, reason_recovery, 0,
                                     true, true));
}

/* Flapping: the flag depends on the reason. */
TEST(ContactViability, Flapping) {
  contact c = all_on();
  c.service_notification_options = flappingstart;  // only accept start

  EXPECT_TRUE(should_notify_contact(c, false, cat_flapping,
                                    reason_flappingstart, 0, true, false));
  EXPECT_FALSE(should_notify_contact(c, false, cat_flapping,
                                     reason_flappingstop, 0, true, false));
}

TEST(ContactViability, Downtime) {
  contact c = all_on();
  EXPECT_TRUE(should_notify_contact(c, false, cat_downtime,
                                    reason_downtimestart, 0, true, false));
  c.service_notification_options = critical;  // no downtime bit
  EXPECT_FALSE(should_notify_contact(c, false, cat_downtime,
                                     reason_downtimestart, 0, true, false));
}

/* Acknowledgement and custom are unconditional (once enabled and in period). */
TEST(ContactViability, AcknowledgementAndCustomUnconditional) {
  contact c = all_on();
  c.service_notification_options = none;  // even with an empty mask

  EXPECT_TRUE(should_notify_contact(c, false, cat_acknowledgement,
                                    reason_acknowledgement, 0, true, false));
  EXPECT_TRUE(should_notify_contact(c, false, cat_custom, reason_custom, 0,
                                    true, false));
}
