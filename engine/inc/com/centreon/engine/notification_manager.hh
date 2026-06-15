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

#ifndef CCE_NOTIFICATION_MANAGER_HH
#define CCE_NOTIFICATION_MANAGER_HH

namespace com::centreon::engine::notification {

/* Status attributes. Used as argument in the notifier::update_status(). */
enum status_attribute {
  STATUS_NONE = 0,
  STATUS_DOWNTIME_DEPTH = 1 << 0,
  STATUS_NOTIFICATION_NUMBER = 1 << 1,
  STATUS_ACKNOWLEDGEMENT = 1 << 2,
  STATUS_ALL = ~0u,
};

enum notification_category {
  cat_normal,
  cat_recovery,
  cat_acknowledgement,
  cat_flapping,
  cat_downtime,
  cat_custom,
};

enum notification_flag {
  none = 0,
  // Host
  up = 1 << 0,
  down = 1 << 1,
  unreachable = 1 << 2,
  // Service
  ok = 1 << 3,
  warning = 1 << 4,
  critical = 1 << 5,
  unknown = 1 << 6,
  // Flapping
  flappingstart = 1 << 7,
  flappingstop = 1 << 8,
  flappingdisabled = 1 << 9,

  // Downtime
  downtime = 1 << 10,
};

enum notifier_type {
  host_notification,
  service_notification,
};

enum reason_type {
  reason_normal,
  reason_recovery,
  reason_acknowledgement,
  reason_flappingstart,
  reason_flappingstop,
  reason_flappingdisabled,
  reason_downtimestart,
  reason_downtimeend,
  reason_downtimecancelled,
  reason_custom = 99,
};

enum notification_option {
  notification_option_none = 0,
  notification_option_broadcast = 1,
  notification_option_forced = 2,
  notification_option_increment = 4,
};

}  // namespace com::centreon::engine::notification

#endif  // !CCE_NOTIFICATION_MANAGER_HH
