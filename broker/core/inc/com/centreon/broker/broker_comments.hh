/**
 * Copyright 2026 Centreon
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
 *
 */

#ifndef CCB_BROKER_COMMENTS_HH
#define CCB_BROKER_COMMENTS_HH

#include <string>

#include "bbdo/neb.pb.h"

/**
 * @brief Comments published by Broker itself (notification_mode = broker).
 *
 * Broker is the comment store: the downtime, acknowledgement and user comments
 * it creates are pb_comment events flowing through the multiplexer to
 * unified_sql, exactly like the ones Engine emits. These free functions are the
 * single place that builds them, so the downtime callbacks, the acknowledgement
 * manager and the comment RPCs all produce the same rows. No state is kept:
 * the ids come from the Broker cache's partitioned range.
 */
namespace com::centreon::broker::broker_comments {

bool is_broker_comment_id(uint64_t internal_id) noexcept;

uint64_t publish_comment(uint64_t host_id,
                         uint64_t service_id,
                         uint32_t instance_id,
                         Comment_EntryType entry_type,
                         Comment_Src source,
                         const std::string& author,
                         const std::string& comment_data,
                         bool persistent,
                         time_t entry_time);

void publish_comment_deletion(uint64_t internal_id, uint32_t instance_id);

void publish_comments_deletion(uint64_t host_id,
                               uint64_t service_id,
                               uint32_t instance_id);

}  // namespace com::centreon::broker::broker_comments

#endif /* !CCB_BROKER_COMMENTS_HH */
