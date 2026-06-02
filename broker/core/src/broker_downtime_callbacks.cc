/**
 * Copyright 2026 Centreon (https://www.centreon.com/)
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

#include "com/centreon/broker/broker_downtime_callbacks.hh"

#include "broker/core/config/applier/state.hh"
#include "com/centreon/broker/multiplexing/publisher.hh"
#include "common/downtimes/downtime_manager.hh"

using namespace com::centreon::common::downtimes;

namespace com::centreon::broker {

/**
 * @brief Construct a broker_downtime_callbacks bound to the given io_context.
 *
 * @param io_context The Boost.Asio io_context used to arm the downtime timer.
 */
broker_downtime_callbacks::broker_downtime_callbacks(
    boost::asio::io_context& io_context)
    : _downtime_timer{io_context} {}

/**
 * @brief Check whether a host is known to the Broker cache.
 *
 * @param host_id The host ID to look up.
 * @return true if the host is in the cache, false otherwise.
 */
bool broker_downtime_callbacks::host_exists(uint64_t host_id) const {
  return config::applier::state::instance().cache().host(host_id) != nullptr;
}

/**
 * @brief Check whether a service is known to the Broker cache.
 *
 * @param host_id    The host ID.
 * @param service_id The service ID.
 * @return true if the service is in the cache, false otherwise.
 */
bool broker_downtime_callbacks::service_exists(uint64_t host_id,
                                               uint64_t service_id) const {
  return config::applier::state::instance().cache().service(
             host_id, service_id) != nullptr;
}

/**
 * @brief Check whether a host or service is known to the Broker cache.
 *
 * @param host_id    The host ID.
 * @param service_id The service ID, or 0 for a host-only check.
 * @return true if the resource exists in the cache.
 */
bool broker_downtime_callbacks::resource_exists(uint64_t host_id,
                                                uint64_t service_id) const {
  return service_id == 0 ? host_exists(host_id)
                         : service_exists(host_id, service_id);
}

/**
 * @brief Check whether a host is UP or a service is OK according to the cache.
 *
 * @param host_id    The host ID.
 * @param service_id The service ID, or 0 for a host check.
 * @return true if the host state is UP or the service state is OK.
 */
bool broker_downtime_callbacks::is_resource_ok(uint64_t host_id,
                                               uint64_t service_id) const {
  auto& cache = config::applier::state::instance().cache();
  if (service_id == 0) {
    auto h = cache.host(host_id);
    return h && h->obj().state() == Host_State_UP;
  }
  auto s = cache.service(host_id, service_id);
  return s && s->obj().state() == Service_State_OK;
}

/**
 * @brief Return the name of a host from the Broker cache.
 *
 * @param host_id The host ID.
 * @return The host name, or an empty string if the host is not in the cache.
 */
std::string broker_downtime_callbacks::get_host_name(uint64_t host_id) const {
  auto h = config::applier::state::instance().cache().host(host_id);
  return h ? h->obj().name() : std::string{};
}

/**
 * @brief Return the host name and service description from the Broker cache.
 *
 * @param host_id    The host ID.
 * @param service_id The service ID.
 * @return A pair {host_name, description}, or {"", ""} if not found.
 */
std::pair<std::string, std::string>
broker_downtime_callbacks::get_host_and_service_names(
    uint64_t host_id,
    uint64_t service_id) const noexcept {
  auto s =
      config::applier::state::instance().cache().service(host_id, service_id);
  return s ? std::make_pair(s->obj().host_name(), s->obj().description())
           : std::make_pair(std::string{}, std::string{});
}

/**
 * @brief No-op: Broker does not create Engine-style comments for downtimes.
 *
 * @return Always 0 (this function does nothing but otherwise it'd return a
 * comment ID).
 */
uint64_t broker_downtime_callbacks::create_downtime_comment(
    uint64_t /*host_id*/,
    uint64_t /*service_id*/,
    const std::string& /*author*/,
    const std::string& /*comment_data*/) {
  return 0;
}

/**
 * @brief No-op: Broker does not manage Engine-style comments for downtimes.
 */
void broker_downtime_callbacks::delete_downtime_comment(
    uint64_t /*comment_id*/) {}

/* --- Anomaly detection lookup --- */

/**
 * @brief Return the service IDs of anomaly detection services monitoring the
 * given dependent service, using the Broker cache index.
 *
 * @param host_id    The host ID of the dependent service.
 * @param service_id The service ID being monitored.
 * @return A vector of anomaly detection service IDs.
 */
std::vector<uint64_t> broker_downtime_callbacks::get_anomaly_detection_services(
    uint64_t host_id,
    uint64_t service_id) const {
  return config::applier::state::instance()
      .cache()
      .find_anomaly_detection_ids_by_dependent_service(host_id, service_id);
}

/* --- Event scheduling --- */

/**
 * @brief Arm the io_context timer to fire at the time of the earliest pending
 * downtime check. Cancels the timer if the schedule is empty.
 */
void broker_downtime_callbacks::_arm_timer() {
  if (_scheduled_downtimes.empty()) {
    _downtime_timer.cancel();
    return;
  }
  auto delay = std::chrono::seconds(std::max(
      0L, (long)(_scheduled_downtimes.begin()->first - time(nullptr))));
  _downtime_timer.expires_after(delay);
  _downtime_timer.async_wait([this](const boost::system::error_code& ec) {
    if (!ec)
      _on_timer();
  });
}

/**
 * @brief Timer callback: process all entries whose fire time is <= now, then
 * rearm for the next entry. A nullptr entry triggers delete_expired_downtimes;
 * a non-null entry calls handle_scheduled_downtime_by_id.
 */
void broker_downtime_callbacks::_on_timer() {
  time_t now = time(nullptr);
  while (!_scheduled_downtimes.empty() &&
         _scheduled_downtimes.begin()->first <= now) {
    auto it = _scheduled_downtimes.begin();
    auto dt = it->second;
    _scheduled_downtimes.erase(it);
    if (dt)
      handle_scheduled_downtime_by_id(dt->get_downtime_id());
    else
      downtime_manager::instance().delete_expired_downtimes();
  }
  _arm_timer();
}

/**
 * @brief Insert a downtime check event into the sorted schedule and rearm the
 * timer if the new entry is earlier than the current head.
 *
 * @param downtime_id The downtime to fire handle_scheduled_downtime_by_id for.
 * @param when        Unix timestamp at which the event should fire.
 */
void broker_downtime_callbacks::schedule_downtime_check(uint64_t downtime_id,
                                                        time_t when) {
  auto dt = downtime_manager::instance().find_downtime(
      downtime::any_downtime, downtime_id);
  if (!dt)
    return;
  bool rearm = _scheduled_downtimes.empty() ||
               when < _scheduled_downtimes.begin()->first;
  _scheduled_downtimes.insert({when, dt});
  if (rearm)
    _arm_timer();
}

/**
 * @brief Schedule a delete_expired_downtimes sweep at the given time, using a
 * nullptr sentinel in the schedule map.
 *
 * @param when Unix timestamp at which the sweep should run.
 */
void broker_downtime_callbacks::schedule_expire_downtime(time_t when) {
  bool rearm = _scheduled_downtimes.empty() ||
               when < _scheduled_downtimes.begin()->first;
  _scheduled_downtimes.insert({when, nullptr});
  if (rearm)
    _arm_timer();
}

/**
 * @brief Remove a pending downtime check event from the schedule. Rearms the
 * timer if the removed entry was at the head of the map.
 *
 * @param downtime_id The downtime whose scheduled event should be cancelled.
 */
void broker_downtime_callbacks::remove_downtime_check(uint64_t downtime_id) {
  for (auto it = _scheduled_downtimes.begin();
       it != _scheduled_downtimes.end(); ++it) {
    if (it->second && it->second->get_downtime_id() == downtime_id) {
      bool was_first = it == _scheduled_downtimes.begin();
      _scheduled_downtimes.erase(it);
      if (was_first)
        _arm_timer();
      return;
    }
  }
}

/**
 * @brief Increment the in-memory pending flexible downtime counter for a host
 * or service. The counter is not persisted to the DB.
 *
 * @param host_id    The host ID.
 * @param service_id The service ID, or 0 for a host downtime.
 * @return true on success, false if the resource is not in the cache.
 */
bool broker_downtime_callbacks::inc_pending_flex_downtime(uint64_t host_id,
                                                          uint64_t service_id) {
  if (!resource_exists(host_id, service_id))
    return false;
  absl::WriterMutexLock l{&_pending_flex_m};
  ++_pending_flex_downtimes[{host_id, service_id}];
  return true;
}

/**
 * @brief Publish a pb_adaptive_host_status or pb_adaptive_service_status event
 * carrying the updated scheduled_downtime_depth to the multiplexer.
 *
 * @param host_id    The host ID.
 * @param service_id The service ID, or 0 for a host.
 * @param depth      The new scheduled_downtime_depth value.
 */
static void _publish_downtime_depth(uint64_t host_id,
                                     uint64_t service_id,
                                     int32_t depth) {
  multiplexing::publisher pblshr;
  if (service_id == 0) {
    auto ev = std::make_shared<neb::pb_adaptive_host_status>();
    ev->mut_obj().set_host_id(host_id);
    ev->mut_obj().set_scheduled_downtime_depth(depth);
    pblshr.write(ev);
  } else {
    auto ev = std::make_shared<neb::pb_adaptive_service_status>();
    ev->mut_obj().set_host_id(host_id);
    ev->mut_obj().set_service_id(service_id);
    ev->mut_obj().set_scheduled_downtime_depth(depth);
    pblshr.write(ev);
  }
}

/**
 * @brief Increment the scheduled_downtime_depth in the cache and publish the
 * updated status so that unified_sql persists it to the DB.
 *
 * @param host_id    The host ID.
 * @param service_id The service ID, or 0 for a host downtime.
 * @param author     Unused in the Broker implementation.
 * @param comment    Unused in the Broker implementation.
 */
void broker_downtime_callbacks::start_downtime_effect(
    uint64_t host_id,
    uint64_t service_id,
    const std::string& /*author*/,
    const std::string& /*comment*/) {
  int32_t depth = config::applier::state::instance().cache().add_downtime(
      host_id, service_id);
  _publish_downtime_depth(host_id, service_id, depth);
}

/**
 * @brief Decrement the scheduled_downtime_depth in the cache, publish the
 * updated status, and decrement the pending flex counter when applicable.
 *
 * @param host_id             The host ID.
 * @param service_id          The service ID, or 0 for a host downtime.
 * @param is_fixed            True if the downtime was fixed.
 * @param incremented_pending True if the pending flex counter was incremented.
 * @param author              Unused in the Broker implementation.
 * @param comment             Unused in the Broker implementation.
 */
void broker_downtime_callbacks::end_downtime_effect(
    uint64_t host_id,
    uint64_t service_id,
    bool is_fixed,
    bool incremented_pending,
    const std::string& /*author*/,
    const std::string& /*comment*/) {
  int32_t depth = config::applier::state::instance().cache().remove_downtime(
      host_id, service_id);
  _publish_downtime_depth(host_id, service_id, depth);
  if (!is_fixed && incremented_pending) {
    absl::WriterMutexLock l{&_pending_flex_m};
    auto it = _pending_flex_downtimes.find({host_id, service_id});
    if (it != _pending_flex_downtimes.end() && it->second > 0) {
      if (--it->second == 0)
        _pending_flex_downtimes.erase(it);
    }
  }
}

/**
 * @brief Cancel the effect of a downtime: decrement the pending flex counter
 * and/or the scheduled downtime depth in the cache, then publish the update.
 *
 * @param host_id             The host ID.
 * @param service_id          The service ID, or 0 for a host downtime.
 * @param is_fixed            True if the downtime was fixed.
 * @param incremented_pending True if the pending flex counter was incremented.
 * @param is_in_effect        True if the downtime was currently active.
 * @return true on success, false if the resource is not in the cache.
 */
bool broker_downtime_callbacks::cancel_downtime(uint64_t host_id,
                                                uint64_t service_id,
                                                bool is_fixed,
                                                bool incremented_pending,
                                                bool is_in_effect) {
  if (!resource_exists(host_id, service_id))
    return false;
  if (!is_fixed && incremented_pending) {
    absl::WriterMutexLock l{&_pending_flex_m};
    auto it = _pending_flex_downtimes.find({host_id, service_id});
    if (it != _pending_flex_downtimes.end() && it->second > 0) {
      if (--it->second == 0)
        _pending_flex_downtimes.erase(it);
    }
  }
  if (is_in_effect) {
    int32_t depth = config::applier::state::instance().cache().remove_downtime(
        host_id, service_id);
    _publish_downtime_depth(host_id, service_id, depth);
  }
  return true;
}

/* --- Broker notification --- */

/**
 * @brief Publish a pb_downtime BBDO event to the multiplexer for each
 * downtime lifecycle transition.
 *
 * Maintains an internal _downtimes cache (guarded by _downtimes_m) so
 * that START/STOP/DELETE transitions can update the same shared_ptr that was
 * published on ADD, avoiding a new allocation per transition. The mutex is
 * released before calling pblshr.write() to avoid holding it during I/O.
 *
 * @param act          Lifecycle action (ADD, LOAD, START, STOP, DELETE).
 * @param attr         Stop reason (NONE, STOP_NORMAL, STOP_CANCELLED).
 * @param host_id      The host ID.
 * @param service_id   The service ID (0 for host downtimes).
 * @param author       The author who scheduled the downtime.
 * @param comment      The downtime comment.
 * @param entry_time   Creation timestamp.
 * @param start_time   Scheduled start time.
 * @param end_time     Scheduled end time.
 * @param fixed        True if the downtime is fixed.
 * @param triggered_by Parent downtime ID (0 if none).
 * @param duration     Duration in seconds (for flexible downtimes).
 * @param downtime_id  Unique downtime identifier.
 */
void broker_downtime_callbacks::notify_broker(action act,
                                              attribute attr,
                                              uint64_t host_id,
                                              uint64_t service_id,
                                              const std::string& author,
                                              const std::string& comment,
                                              time_t entry_time,
                                              time_t start_time,
                                              time_t end_time,
                                              bool fixed,
                                              uint64_t triggered_by,
                                              uint32_t duration,
                                              uint64_t downtime_id) {
  multiplexing::publisher pblshr;
  std::shared_ptr<neb::pb_downtime> pb_dt;

  {
    absl::WriterMutexLock lk{&_downtimes_m};

    if (act == ADD || act == LOAD) {
      pb_dt = std::make_shared<neb::pb_downtime>();
      auto& obj = pb_dt->mut_obj();
      obj.set_id(downtime_id);
      obj.set_host_id(host_id);
      obj.set_service_id(service_id);
      auto& cache = config::applier::state::instance().cache();
      auto h = cache.host(host_id);
      uint32_t inst_id = (h && h->obj().instance_id() != 0)
                             ? h->obj().instance_id()
                             : cache.first_active_instance_id();
      if (inst_id != 0)
        obj.set_instance_id(inst_id);
      obj.set_author(author);
      obj.set_comment_data(comment);
      obj.set_entry_time(entry_time);
      obj.set_start_time(start_time);
      obj.set_end_time(end_time);
      obj.set_fixed(fixed);
      obj.set_triggered_by(triggered_by);
      obj.set_duration(duration);
      obj.set_type(service_id == 0 ? Downtime_DowntimeType_HOST
                                   : Downtime_DowntimeType_SERVICE);
      obj.set_started(false);
      obj.set_cancelled(false);
      obj.set_actual_start_time(-1);
      obj.set_actual_end_time(-1);
      obj.set_deletion_time(-1);
      _downtimes[downtime_id] = pb_dt;
    } else {
      auto it = _downtimes.find(downtime_id);
      if (it == _downtimes.end())
        return;
      pb_dt = it->second;

      if (act == START) {
        pb_dt->mut_obj().set_started(true);
        pb_dt->mut_obj().set_actual_start_time(time(nullptr));
      } else if (act == STOP) {
        pb_dt->mut_obj().set_cancelled(attr ==
                                       attribute::ATTR_STOP_CANCELLED);
        pb_dt->mut_obj().set_actual_end_time(time(nullptr));
        pb_dt->mut_obj().set_deletion_time(time(nullptr));
        _downtimes.erase(it);
      } else if (act == DELETE) {
        pb_dt->mut_obj().set_deletion_time(time(nullptr));
        pb_dt->mut_obj().set_cancelled(true);
        _downtimes.erase(it);
      }
    }
  }  // release lock before publishing

  pblshr.write(pb_dt);
}

}  // namespace com::centreon::broker
