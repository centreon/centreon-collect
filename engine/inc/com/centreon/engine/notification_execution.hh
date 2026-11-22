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

#ifndef CCE_NOTIFICATION_EXECUTION_HH
#define CCE_NOTIFICATION_EXECUTION_HH

#include <cstdint>
#include <memory>
#include <string>

#include <absl/container/btree_set.h>
#include <absl/container/flat_hash_set.h>
#include <google/protobuf/repeated_ptr_field.h>

#include "common/notifications/notification_types.hh"

namespace com::centreon::engine {

class contact;
class notifier;

/* The EXECUTION side of a notification (macro expansion + notification command
 * launch), shared by both notification modes and kept separate from the two
 * notification_callbacks backends (which are the library's DECISION side):
 *  - engine mode: engine_notification_callbacks::deliver() selects the contacts
 *    then calls run_notification_commands();
 *  - broker mode: Broker decides and dispatches a pb_notification_execute to the
 *    supervising poller, whose event loop calls execute_broker_notification().
 * Both ultimately run on the poller because macros need the resource's runtime
 * objects. */

absl::btree_set<std::string> run_notification_commands(
    notifier* n,
    const absl::flat_hash_set<std::shared_ptr<contact>>& to_notify,
    common::notifications::reason_type type,
    uint64_t notification_id,
    uint32_t notification_number,
    bool escalated,
    const std::string& author,
    const std::string& message,
    common::notifications::notification_option options);

void execute_broker_notification(
    uint64_t host_id,
    uint64_t service_id,
    common::notifications::reason_type type,
    uint64_t notification_id,
    uint32_t notification_number,
    bool escalated,
    const std::string& author,
    const std::string& message,
    common::notifications::notification_option options,
    const ::google::protobuf::RepeatedPtrField<std::string>& contacts);

}  // namespace com::centreon::engine

#endif  // !CCE_NOTIFICATION_EXECUTION_HH
