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

#ifndef CCC_NOTIFICATIONS_CONTACT_VIABILITY_HH
#define CCC_NOTIFICATIONS_CONTACT_VIABILITY_HH

#include "common/notifications/notification_types.hh"

namespace com::centreon::common::notifications {

bool should_notify_contact(const contact& c,
                           bool is_host,
                           notification_category cat,
                           reason_type type,
                           int current_state,
                           bool in_period,
                           bool already_notified);

}  // namespace com::centreon::common::notifications

#endif  // !CCC_NOTIFICATIONS_CONTACT_VIABILITY_HH
