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

#include "com/centreon/broker/broker_acknowledgement_manager.hh"

#include <fmt/format.h>

#include "broker/core/config/applier/state.hh"
#include "com/centreon/broker/multiplexing/publisher.hh"
#include "common/log_v2/log_v2.hh"
#include "common/notifications/notification_manager.hh"

using log_v2 = com::centreon::common::log_v2::log_v2;
namespace notifications = com::centreon::common::notifications;

namespace com::centreon::broker {

std::unique_ptr<broker_acknowledgement_manager>
    broker_acknowledgement_manager::_instance;

broker_acknowledgement_manager::broker_acknowledgement_manager()
    : _logger{log_v2::instance().get(log_v2::CORE)} {}

/**
 * @brief Create the singleton (notification_mode = broker only).
 */
void broker_acknowledgement_manager::load() {
  if (!_instance)
    _instance.reset(new broker_acknowledgement_manager);
}

/**
 * @brief Destroy the singleton.
 */
void broker_acknowledgement_manager::unload() {
  _instance.reset();
}

/**
 * @brief Access the singleton. load() must have been called.
 *
 * @return The acknowledgement manager.
 */
broker_acknowledgement_manager& broker_acknowledgement_manager::instance() {
  assert(_instance);
  return *_instance;
}

/**
 * @brief Publish the acknowledgement comment (entry_type ACKNOWLEDGMENT,
 * source INTERNAL), the way Engine creates one in acknowledge_host_problem().
 *
 * The internal_id is drawn from Broker's partitioned range so it never
 * collides with the ids Engine mints; instance_id is the host's poller so the
 * row is addressable by unified_sql.
 *
 * @param host_id      The host id of the acknowledged resource.
 * @param service_id   The service id, 0 for a host.
 * @param instance_id  The poller id of the host.
 * @param author       The acknowledgement author.
 * @param comment_data The acknowledgement text.
 * @param persistent   Whether the comment must survive the acknowledgement.
 * @param entry_time   The acknowledgement entry time, shared with the
 *                     acknowledgement row the GUI joins the comment on.
 *
 * @return The internal_id of the comment, kept on the acknowledgement.
 */
uint64_t broker_acknowledgement_manager::_create_comment(
    uint64_t host_id,
    uint64_t service_id,
    uint32_t instance_id,
    const std::string& author,
    const std::string& comment_data,
    bool persistent,
    time_t entry_time) {
  auto& cache = config::applier::state::instance().cache();
  uint64_t internal_id = cache.next_downtime_comment_id();

  auto ev = std::make_shared<neb::pb_comment>();
  auto& obj = ev->mut_obj();
  obj.set_author(author);
  obj.set_type(service_id == 0 ? Comment_Type_HOST : Comment_Type_SERVICE);
  obj.set_data(comment_data);
  /* Same entry_time as the acknowledgement: the GUI joins both rows on it. */
  obj.set_entry_time(entry_time);
  obj.set_entry_type(Comment_EntryType_ACKNOWLEDGMENT);
  obj.set_host_id(host_id);
  obj.set_service_id(service_id);
  obj.set_internal_id(internal_id);
  obj.set_persistent(persistent);
  obj.set_instance_id(instance_id);
  obj.set_source(Comment_Src_INTERNAL);

  multiplexing::publisher pblshr;
  pblshr.write(ev);
  return internal_id;
}

/**
 * @brief Publish the deletion of an acknowledgement comment (a pb_comment
 * carrying only internal_id/instance_id/deletion_time, like
 * broker_downtime_callbacks::delete_downtime_comment). No-op when comment_id
 * is 0.
 *
 * @param comment_id  The internal_id of the comment to delete.
 * @param instance_id The poller id the comment was created with.
 */
void broker_acknowledgement_manager::_delete_comment(uint64_t comment_id,
                                                     uint32_t instance_id) {
  if (comment_id == 0)
    return;
  auto ev = std::make_shared<neb::pb_comment>();
  auto& obj = ev->mut_obj();
  obj.set_internal_id(comment_id);
  obj.set_instance_id(instance_id);
  obj.set_deletion_time(time(nullptr));

  multiplexing::publisher pblshr;
  pblshr.write(ev);
}

/**
 * @brief Publish the acknowledgement type through an adaptive status: this is
 * what Engine's update_status(STATUS_ACKNOWLEDGEMENT) emits, so unified_sql
 * updates hosts/services/resources the same way in both modes.
 *
 * @param host_id    The host id of the resource.
 * @param service_id The service id, 0 for a host.
 * @param type       The acknowledgement type to publish (NONE to clear).
 */
void broker_acknowledgement_manager::_publish_ack_type(uint64_t host_id,
                                                       uint64_t service_id,
                                                       AckType type) {
  multiplexing::publisher pblshr;
  if (service_id == 0) {
    auto ev = std::make_shared<neb::pb_adaptive_host_status>();
    auto& obj = ev->mut_obj();
    obj.set_host_id(host_id);
    obj.set_acknowledgement_type(type);
    pblshr.write(ev);
  } else {
    auto ev = std::make_shared<neb::pb_adaptive_service_status>();
    auto& obj = ev->mut_obj();
    obj.set_host_id(host_id);
    obj.set_service_id(service_id);
    obj.set_acknowledgement_type(type);
    pblshr.write(ev);
  }
}

/**
 * @brief Publish the `logs` table entry Engine would have produced from the
 * external command line (msg_type SERVICE/HOST_ACKNOWLEDGE_PROBLEM, author in
 * notification_contact, comment in output), so the GUI event log is unchanged.
 * No-op when the host is unknown to the cache.
 *
 * @param host_id     The host id of the resource.
 * @param service_id  The service id, 0 for a host.
 * @param instance_id The poller id, resolved to its name for the entry.
 * @param author      The acknowledgement author (notification_contact column).
 * @param output      The external command line Engine would have logged.
 * @param msg_type    SERVICE_ACKNOWLEDGE_PROBLEM or HOST_ACKNOWLEDGE_PROBLEM.
 */
void broker_acknowledgement_manager::_publish_log(uint64_t host_id,
                                                  uint64_t service_id,
                                                  uint32_t instance_id,
                                                  const std::string& author,
                                                  const std::string& output,
                                                  LogEntry_MsgType msg_type) {
  auto& cache = config::applier::state::instance().cache();
  auto h = cache.host(host_id);
  if (!h)
    return;

  auto le = std::make_shared<neb::pb_log_entry>();
  auto& obj = le->mut_obj();
  obj.set_ctime(time(nullptr));
  obj.set_instance_name(cache.instance_name(instance_id));
  obj.set_host_id(host_id);
  obj.set_host_name(h->obj().name());
  if (service_id) {
    auto s = cache.service(host_id, service_id);
    if (s) {
      obj.set_service_id(service_id);
      obj.set_service_description(s->obj().description());
    }
  }
  obj.set_notification_contact(author);
  obj.set_output(output);
  obj.set_msg_type(msg_type);

  multiplexing::publisher pblshr;
  pblshr.write(le);
}

/**
 * @brief Acknowledge a host (service_id == 0) or service problem.
 *
 * Mirrors Engine's acknowledge_host_problem()/acknowledge_service_problem():
 * refuse when the resource is UP/OK, set the flag, emit the acknowledgement
 * event, notify if asked, export the status and add the comment. The cache is
 * updated synchronously so the very next notification decision sees the
 * acknowledged flag.
 *
 * @param host_id      The host id of the resource.
 * @param service_id   The service id, 0 for a host.
 * @param author       The acknowledgement author.
 * @param comment_data The acknowledgement text.
 * @param sticky       True for a STICKY acknowledgement (kept until recovery),
 *                     false for a NORMAL one (cleared on any state change).
 * @param notify       Whether to send the acknowledgement notification.
 * @param persistent   Whether the comment survives the acknowledgement.
 *
 * @return An empty string on success, the error message otherwise.
 */
std::string broker_acknowledgement_manager::acknowledge(
    uint64_t host_id,
    uint64_t service_id,
    const std::string& author,
    const std::string& comment_data,
    bool sticky,
    bool notify,
    bool persistent) {
  auto& cache = config::applier::state::instance().cache();
  auto h = cache.host(host_id);
  if (!h)
    return fmt::format("could not find host {}", host_id);
  const uint32_t instance_id = h->obj().instance_id();

  uint32_t state = 0;
  std::string output;
  if (service_id) {
    auto s = cache.service(host_id, service_id);
    if (!s)
      return fmt::format("could not find service ({}, {})", host_id,
                         service_id);
    const auto& obj = s->obj();
    state = obj.state();
    if (state == Service_State_OK)
      return fmt::format("state of service '{}' on host '{}' is ok",
                         obj.description(), obj.host_name());
    output =
        fmt::format("ACKNOWLEDGE_SVC_PROBLEM;{};{};{};{};{};{};{}",
                    obj.host_name(), obj.description(), sticky ? 2 : 1,
                    notify ? 1 : 0, persistent ? 1 : 0, author, comment_data);
  } else {
    const auto& obj = h->obj();
    state = obj.state();
    if (state == Host_State_UP)
      return fmt::format("state of host '{}' is up", obj.name());
    output = fmt::format("ACKNOWLEDGE_HOST_PROBLEM;{};{};{};{};{};{}",
                         obj.name(), sticky ? 2 : 1, notify ? 1 : 0,
                         persistent ? 1 : 0, author, comment_data);
  }

  /* A new acknowledgement replaces the previous one: like Engine, drop the
   * previous non-persistent comment. */
  auto prev = cache.acknowledgement(host_id, service_id);
  if (prev && !prev->obj().persistent_comment())
    _delete_comment(prev->obj().comment_id(), instance_id);

  const time_t now = time(nullptr);
  const uint64_t comment_id = _create_comment(
      host_id, service_id, instance_id, author, comment_data, persistent, now);

  auto ack = std::make_shared<neb::pb_acknowledgement>();
  auto& obj = ack->mut_obj();
  obj.set_type(service_id == 0 ? Acknowledgement_ResourceType_HOST
                               : Acknowledgement_ResourceType_SERVICE);
  obj.set_host_id(host_id);
  obj.set_service_id(service_id);
  obj.set_instance_id(instance_id);
  obj.set_author(author);
  obj.set_comment_data(comment_data);
  obj.set_entry_time(now);
  obj.set_sticky(sticky);
  obj.set_notify_contacts(notify);
  obj.set_persistent_comment(persistent);
  obj.set_state(state);
  obj.set_comment_id(comment_id);

  const AckType type = sticky ? AckType::STICKY : AckType::NORMAL;
  /* Synchronous: the notification viability reads this flag from the cache. */
  cache.set_acknowledgement_type(host_id, service_id, type);

  SPDLOG_LOGGER_INFO(_logger,
                     "acknowledgement of ({}, {}) by '{}' (sticky: {}, notify: "
                     "{}, persistent: {})",
                     host_id, service_id, author, sticky, notify, persistent);

  {
    multiplexing::publisher pblshr;
    pblshr.write(ack);
  }
  _publish_ack_type(host_id, service_id, type);

  if (notify && notifications::notification_manager::is_loaded())
    notifications::notification_manager::instance().notify(
        host_id, service_id, notifications::reason_acknowledgement, author,
        comment_data, notifications::notification_option_none);

  _publish_log(host_id, service_id, instance_id, author, output,
               service_id == 0 ? LogEntry_MsgType_HOST_ACKNOWLEDGE_PROBLEM
                               : LogEntry_MsgType_SERVICE_ACKNOWLEDGE_PROBLEM);
  return "";
}

/**
 * @brief Remove a host (service_id == 0) or service acknowledgement.
 *
 * Mirrors Engine's remove_host_acknowledgement(): flag to NONE, status export,
 * deletion of the non-persistent comment. The acknowledgement row gets its
 * deletion_time (the resource is still in a problem state).
 *
 * @param host_id    The host id of the resource.
 * @param service_id The service id, 0 for a host.
 *
 * @return An empty string on success, the error message otherwise.
 */
std::string broker_acknowledgement_manager::remove(uint64_t host_id,
                                                   uint64_t service_id) {
  auto& cache = config::applier::state::instance().cache();
  auto h = cache.host(host_id);
  if (!h)
    return fmt::format("could not find host {}", host_id);
  if (service_id && !cache.service(host_id, service_id))
    return fmt::format("could not find service ({}, {})", host_id, service_id);
  const uint32_t instance_id = h->obj().instance_id();

  auto ack = cache.acknowledgement(host_id, service_id);
  auto closed =
      cache.set_acknowledgement_type(host_id, service_id, AckType::NONE);
  SPDLOG_LOGGER_INFO(_logger, "acknowledgement of ({}, {}) removed", host_id,
                     service_id);
  if (closed) {
    multiplexing::publisher pblshr;
    pblshr.write(std::move(closed));
  }
  if (ack && !ack->obj().persistent_comment())
    _delete_comment(ack->obj().comment_id(), instance_id);
  _publish_ack_type(host_id, service_id, AckType::NONE);
  return "";
}

/**
 * @brief Clear the acknowledgement of a resource when its new state requires
 * it (automatic clearing rule applied on every host/service status).
 *
 * Same rule as Engine's notifier::handle_state(): a NORMAL acknowledgement is
 * cleared on any state change, a STICKY one only on recovery (UP/OK). Nothing
 * happens when the resource is not acknowledged or when the state keeps the
 * acknowledgement. Called by broker_notification_dispatcher for every status
 * event before the notification decision, so a state change out of a
 * non-sticky acknowledgement gets notified like Engine does.
 *
 * @param host_id    The host id.
 * @param service_id The service id, 0 for a host.
 * @param state      The state carried by the status event.
 */
void broker_acknowledgement_manager::clear_on_state_change(uint64_t host_id,
                                                           uint64_t service_id,
                                                           uint32_t state) {
  auto& cache = config::applier::state::instance().cache();
  auto ack = cache.acknowledgement(host_id, service_id);
  if (!ack)
    return;
  const auto& o = ack->obj();
  if (state != 0 && (o.sticky() || state == o.state()))
    return;

  SPDLOG_LOGGER_INFO(_logger,
                     "acknowledgement of ({}, {}) cleared: state {} -> {}"
                     " (sticky: {})",
                     host_id, service_id, o.state(), state, o.sticky());
  auto closed = cache.set_acknowledgement_type(
      host_id, service_id, AckType::NONE, static_cast<uint16_t>(state));
  if (closed) {
    multiplexing::publisher pblshr;
    pblshr.write(std::move(closed));
  }
  if (!o.persistent_comment()) {
    auto h = cache.host(host_id);
    _delete_comment(o.comment_id(), h ? h->obj().instance_id() : 0);
  }
  _publish_ack_type(host_id, service_id, AckType::NONE);
}

}  // namespace com::centreon::broker
