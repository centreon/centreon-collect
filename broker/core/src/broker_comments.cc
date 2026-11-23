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

#include "com/centreon/broker/broker_comments.hh"

#include "broker/core/config/applier/state.hh"
#include "com/centreon/broker/multiplexing/publisher.hh"

namespace com::centreon::broker::broker_comments {

/**
 * @brief Tell whether a comment internal_id was minted by Broker.
 *
 * Broker draws its ids from a range partitioned away from the ones the
 * pollers mint (see broker_cache::comment_id_base), with a single counter for
 * the whole platform: such an id identifies its row alone, without the
 * instance_id an Engine-minted id would need.
 *
 * @param internal_id The comment internal_id.
 *
 * @return True when the id is in Broker's range.
 */
bool is_broker_comment_id(uint64_t internal_id) noexcept {
  return internal_id >= cache::broker_cache::comment_id_base;
}

/**
 * @brief Publish a comment created by Broker.
 *
 * The internal_id is drawn from Broker's partitioned range so it never
 * collides with the ids Engine mints; instance_id is the resource's poller so
 * the row is addressable by unified_sql the way an Engine comment is.
 *
 * @param host_id      The host id of the commented resource.
 * @param service_id   The service id, 0 for a host comment.
 * @param instance_id  The poller id of the host.
 * @param entry_type   USER, DOWNTIME or ACKNOWLEDGMENT.
 * @param source       INTERNAL (created by Broker on its own) or EXTERNAL
 *                     (requested through the API).
 * @param author       The comment author.
 * @param comment_data The comment text.
 * @param persistent   Whether the comment survives the object it documents
 *                     (downtime end, acknowledgement clearing).
 * @param entry_time   The comment entry time.
 *
 * @return The internal_id of the published comment.
 */
uint64_t publish_comment(uint64_t host_id,
                         uint64_t service_id,
                         uint32_t instance_id,
                         Comment_EntryType entry_type,
                         Comment_Src source,
                         const std::string& author,
                         const std::string& comment_data,
                         bool persistent,
                         time_t entry_time) {
  auto& cache = config::applier::state::instance().cache();
  const uint64_t internal_id = cache.next_downtime_comment_id();

  auto ev = std::make_shared<neb::pb_comment>();
  auto& obj = ev->mut_obj();
  obj.set_author(author);
  obj.set_type(service_id == 0 ? Comment_Type_HOST : Comment_Type_SERVICE);
  obj.set_data(comment_data);
  obj.set_entry_time(entry_time);
  obj.set_entry_type(entry_type);
  obj.set_host_id(host_id);
  obj.set_service_id(service_id);
  obj.set_internal_id(internal_id);
  obj.set_persistent(persistent);
  obj.set_instance_id(instance_id);
  obj.set_source(source);

  multiplexing::publisher pblshr;
  pblshr.write(ev);
  return internal_id;
}

/**
 * @brief Publish the deletion of one comment.
 *
 * The event carries only internal_id, instance_id and deletion_time:
 * unified_sql turns it into an UPDATE of the matching row. With instance_id 0
 * the row is matched by internal_id alone, which is only valid for an id
 * minted by Broker (see is_broker_comment_id()). No-op when internal_id is 0.
 *
 * @param internal_id The comment internal_id.
 * @param instance_id The poller id the comment was created with, or 0 to match
 *                    by internal_id alone.
 */
void publish_comment_deletion(uint64_t internal_id, uint32_t instance_id) {
  if (internal_id == 0)
    return;
  auto ev = std::make_shared<neb::pb_comment>();
  auto& obj = ev->mut_obj();
  obj.set_internal_id(internal_id);
  obj.set_instance_id(instance_id);
  obj.set_deletion_time(time(nullptr));

  multiplexing::publisher pblshr;
  pblshr.write(ev);
}

/**
 * @brief Publish the deletion of every comment of a host or service.
 *
 * Same event Engine emits for DEL_ALL_*_COMMENTS: internal_id 0 is the bulk
 * sentinel, unified_sql matches on host_id (+ service_id) and instance_id.
 * Every comment of the resource is deleted whatever its entry type, like
 * Engine does.
 *
 * @param host_id     The host id.
 * @param service_id  The service id, 0 to delete the host's own comments.
 * @param instance_id The poller id of the host.
 */
void publish_comments_deletion(uint64_t host_id,
                               uint64_t service_id,
                               uint32_t instance_id) {
  auto ev = std::make_shared<neb::pb_comment>();
  auto& obj = ev->mut_obj();
  obj.set_type(service_id == 0 ? Comment_Type_HOST : Comment_Type_SERVICE);
  obj.set_host_id(host_id);
  obj.set_service_id(service_id);
  obj.set_instance_id(instance_id);
  obj.set_deletion_time(time(nullptr));

  multiplexing::publisher pblshr;
  pblshr.write(ev);
}

}  // namespace com::centreon::broker::broker_comments
