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

#include "com/centreon/engine/engine_downtime_callbacks.hh"

#include "com/centreon/engine/anomalydetection.hh"
#include "com/centreon/engine/broker.hh"
#include "com/centreon/engine/comment.hh"
#include "com/centreon/engine/events/loop.hh"
#include "com/centreon/engine/events/timed_event.hh"
#include "com/centreon/engine/host.hh"
#include "com/centreon/engine/service.hh"

using namespace com::centreon::common::downtimes;

namespace com::centreon::engine {
/**
 * @brief Check whether a host with the given ID exists in the Engine runtime.
 *
 * @param host_id The host ID to look up.
 * @return true if the host is known to Engine, false otherwise.
 */
bool engine_downtime_callbacks::host_exists(uint64_t host_id) const {
  return host::hosts_by_id.contains(host_id);
}

/**
 * @brief Check whether a service with the given host/service ID pair exists in
 * the Engine runtime.
 *
 * @param host_id    The host ID.
 * @param service_id The service ID.
 * @return true if the service is known to Engine, false otherwise.
 */
bool engine_downtime_callbacks::service_exists(uint64_t host_id,
                                               uint64_t service_id) const {
  return service::services_by_id.contains({host_id, service_id});
}

/**
 * @brief Get the host name from its id.
 *
 * @param host_id The host id.
 *
 * @return The host name or an empty string if the host does not exist.
 */
std::string engine_downtime_callbacks::get_host_name(uint64_t host_id) const {
  auto found = host::hosts_by_id.find(host_id);
  if (found != host::hosts_by_id.end())
    return found->second->name();
  return {};
}

/**
 * @brief Get the host and service names from their ids.
 *
 * @param host_id The host id.
 * @param service_id The service id.
 *
 * @return A pair containing the host name and the service description, or a
 * pair of empty strings if the service does not exist.
 */
std::pair<std::string, std::string>
engine_downtime_callbacks::get_host_and_service_names(
    uint64_t host_id,
    uint64_t service_id) const noexcept {
  auto it = service::services_by_id.find({host_id, service_id});
  if (it != service::services_by_id.end())
    return std::make_pair(it->second->get_hostname(),
                          it->second->description());
  else
    return {{}, {}};
}

/**
 * @brief Return the service IDs of anomaly detection services that monitor
 * the given dependent service.
 *
 * @param host_id    The host ID of the dependent service.
 * @param service_id The service ID of the service being monitored.
 * @return A vector of anomaly detection service IDs.
 */
std::vector<uint64_t> engine_downtime_callbacks::get_anomaly_detection_services(
    uint64_t host_id,
    uint64_t service_id) const {
  std::vector<uint64_t> result;
  for (const anomalydetection* ano :
       anomalydetection::find_by_dependent_service(host_id, service_id))
    result.push_back(ano->service_id());
  return result;
}

/**
 * @brief Cancel the effect of an active downtime on a host or service.
 *
 * Decrements the pending flex downtime counter if applicable, decrements the
 * scheduled downtime depth when the downtime was in effect, and sends a
 * cancellation notification when the depth reaches zero.
 *
 * @param host_id             The host ID.
 * @param service_id          The service ID (0 for a host downtime).
 * @param is_fixed            True if the downtime was fixed.
 * @param incremented_pending True if the pending flex counter was incremented.
 * @param is_in_effect        True if the downtime was currently active.
 * @return true on success, false if the host or service is not found.
 */
bool engine_downtime_callbacks::cancel_downtime(uint64_t host_id,
                                                uint64_t service_id,
                                                bool is_fixed,
                                                bool incremented_pending,
                                                bool is_in_effect) {
  if (service_id == 0) {
    auto it = host::hosts_by_id.find(host_id);
    if (it == host::hosts_by_id.end() || it->second == nullptr)
      return false;
    if (!is_fixed && incremented_pending)
      it->second->dec_pending_flex_downtime();
    if (is_in_effect) {
      it->second->dec_scheduled_downtime_depth();
      it->second->update_status();
      if (it->second->get_scheduled_downtime_depth() == 0) {
        _logger->info(
            "HOST DOWNTIME ALERT: {};CANCELLED; Scheduled downtime for host "
            "has been cancelled.",
            it->second->name());
        it->second->notify(notifier::reason_downtimecancelled, "", "",
                           notifier::notification_option_none);
      }
    }
  } else {
    auto found = service::services_by_id.find({host_id, service_id});
    if (found == service::services_by_id.end() || !found->second)
      return false;
    if (!is_fixed && incremented_pending)
      found->second->dec_pending_flex_downtime();
    if (is_in_effect) {
      found->second->dec_scheduled_downtime_depth();
      found->second->update_status(service::STATUS_DOWNTIME_DEPTH);
      if (found->second->get_scheduled_downtime_depth() == 0) {
        _logger->info(
            "SERVICE DOWNTIME ALERT: {};{};CANCELLED; Scheduled downtime for "
            "service has been cancelled.",
            found->second->get_hostname(), found->second->description());
        found->second->notify(notifier::reason_downtimecancelled, "", "",
                              notifier::notification_option_none);
      }
    }
  }
  return true;
}

/**
 * @brief Schedule an EVENT_SCHEDULED_DOWNTIME event in the Engine event loop.
 *
 * Called by downtime::subscribe() to schedule the downtime start, and by
 * downtime::handle() (START path) to schedule the downtime end.
 *
 * @param downtime_id The downtime ID passed as event data.
 * @param when        The Unix timestamp at which the event should fire.
 */
void engine_downtime_callbacks::schedule_downtime_check(uint64_t downtime_id,
                                                        time_t when) {
  uint64_t* id = new uint64_t{downtime_id};
  events::loop::instance().schedule(
      std::make_unique<timed_event>(timed_event::EVENT_SCHEDULED_DOWNTIME, when,
                                    false, 0, nullptr, false, (void*)id,
                                    nullptr, 0),
      true);
}

/**
 * @brief Schedule an EVENT_EXPIRE_DOWNTIME event in the Engine event loop.
 *
 * Called when a flexible downtime window closes without the monitored object
 * having changed state; the event triggers delete_expired_downtimes().
 *
 * @param when The Unix timestamp at which the event should fire.
 */
void engine_downtime_callbacks::schedule_expire_downtime(time_t when) {
  events::loop::instance().schedule(
      std::make_unique<timed_event>(timed_event::EVENT_EXPIRE_DOWNTIME, when,
                                    false, 0, nullptr, false, nullptr, nullptr,
                                    0),
      true);
}

/**
 * @brief Remove a pending EVENT_SCHEDULED_DOWNTIME event from the Engine event
 * loop.
 *
 * Called when a downtime is unscheduled before its check event fires.
 *
 * @param downtime_id The downtime ID whose event should be removed.
 */
void engine_downtime_callbacks::remove_downtime_check(uint64_t downtime_id) {
  events::loop::instance().remove_downtime(downtime_id);
}

/**
 * @brief Publish a downtime lifecycle event to Broker via the NEB callback.
 *
 * Translates the abstract action/attribute enums to NEBTYPE_DOWNTIME_* and
 * NEBATTR_* constants and calls broker_downtime_data().
 *
 * @param act          The lifecycle action (ADD, START, STOP, DELETE, LOAD).
 * @param attr         Stop reason attribute (NONE, STOP_NORMAL, STOP_CANCELLED).
 * @param host_id      The host ID.
 * @param service_id   The service ID (0 for host downtimes).
 * @param author       The author who scheduled the downtime.
 * @param comment      The comment associated with the downtime.
 * @param entry_time   Creation time of the downtime.
 * @param start_time   Scheduled start time.
 * @param end_time     Scheduled end time.
 * @param fixed        True if the downtime is fixed.
 * @param triggered_by Parent downtime ID (0 if none).
 * @param duration     Duration in seconds (for flexible downtimes).
 * @param downtime_id  Unique identifier of the downtime.
 */
void engine_downtime_callbacks::notify_broker(action act,
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
  /* send data to event broker */
  int engine_action;
  switch (act) {
    case ADD:
      engine_action = NEBTYPE_DOWNTIME_ADD;
      break;
    case START:
      engine_action = NEBTYPE_DOWNTIME_START;
      break;
    case STOP:
      engine_action = NEBTYPE_DOWNTIME_STOP;
      break;
    case DELETE:
      engine_action = NEBTYPE_DOWNTIME_DELETE;
      break;
    case LOAD:
      engine_action = NEBTYPE_DOWNTIME_LOAD;
      break;
  }

  int engine_attr = static_cast<int>(attr);
  broker_downtime_data(
      engine_action, engine_attr,
      service_id == 0 ? downtime::host_downtime : downtime::service_downtime,
      host_id, service_id, entry_time, author.c_str(), comment.c_str(),
      start_time, end_time, fixed, triggered_by, duration, downtime_id);
}

/**
 * @brief Check whether a host or service exists in the Engine runtime.
 *
 * Delegates to host_exists() when service_id is 0, and to service_exists()
 * otherwise.
 *
 * @param host_id    The host ID.
 * @param service_id The service ID, or 0 for a host-only check.
 * @return true if the resource exists, false otherwise.
 */
bool engine_downtime_callbacks::resource_exists(uint64_t host_id,
                                                uint64_t service_id) const {
  return service_id == 0 ? host_exists(host_id)
                         : service_exists(host_id, service_id);
}

/**
 * @brief Check whether a host is UP or a service is OK.
 *
 * Used by the downtime manager to decide whether a flexible downtime should
 * be held (object is OK/UP) or started immediately.
 *
 * @param host_id    The host ID.
 * @param service_id The service ID, or 0 for a host check.
 * @return true if the host is UP or the service is OK, false otherwise.
 */
bool engine_downtime_callbacks::is_resource_ok(uint64_t host_id,
                                               uint64_t service_id) const {
  if (service_id == 0) {
    auto it = host::hosts_by_id.find(host_id);
    return it != host::hosts_by_id.end() && it->second &&
           it->second->get_current_state() == host::state_up;
  }
  auto found = service::services_by_id.find({host_id, service_id});
  return found != service::services_by_id.end() && found->second &&
         found->second->get_current_state() == service::state_ok;
}

/**
 * @brief Increment the pending flexible downtime counter on a host or service.
 *
 * Called when a flexible downtime is registered but waiting for the object to
 * enter a non-OK/non-UP state before becoming active.
 *
 * @param host_id    The host ID.
 * @param service_id The service ID, or 0 for a host downtime.
 * @return true on success, false if the host or service is not found.
 */
bool engine_downtime_callbacks::inc_pending_flex_downtime(uint64_t host_id,
                                                          uint64_t service_id) {
  if (service_id == 0) {
    auto it = host::hosts_by_id.find(host_id);
    if (it == host::hosts_by_id.end() || !it->second)
      return false;
    it->second->inc_pending_flex_downtime();
  } else {
    auto found = service::services_by_id.find({host_id, service_id});
    if (found == service::services_by_id.end() || !found->second)
      return false;
    found->second->inc_pending_flex_downtime();
  }
  return true;
}

/**
 * @brief Apply the effect of a downtime becoming active on a host or service.
 *
 * Increments the scheduled downtime depth, sends a DOWNTIME STARTED
 * notification when the depth crosses from 0 to 1, and updates the object
 * status so that Broker receives the new depth.
 *
 * @param host_id    The host ID.
 * @param service_id The service ID, or 0 for a host downtime.
 * @param author     The author who scheduled the downtime (for the notification).
 * @param comment    The downtime comment (for the notification).
 */
void engine_downtime_callbacks::start_downtime_effect(
    uint64_t host_id,
    uint64_t service_id,
    const std::string& author,
    const std::string& comment) {
  if (service_id == 0) {
    auto it = host::hosts_by_id.find(host_id);
    if (it == host::hosts_by_id.end() || !it->second)
      return;
    if (it->second->get_scheduled_downtime_depth() == 0) {
      _logger->trace("Host '{}' has entered a period of scheduled downtime.",
                     it->second->name());
      _logger->info(
          "HOST DOWNTIME ALERT: {};STARTED; Host has entered a period of "
          "scheduled downtime",
          it->second->name());
      it->second->notify(notifier::reason_downtimestart, author, comment,
                         notifier::notification_option_none);
    }
    it->second->inc_scheduled_downtime_depth();
    it->second->update_status(host::STATUS_DOWNTIME_DEPTH);
  } else {
    auto found = service::services_by_id.find({host_id, service_id});
    if (found == service::services_by_id.end() || !found->second)
      return;
    if (found->second->get_scheduled_downtime_depth() == 0) {
      _logger->trace(
          "Service '{}' on host '{}' has entered a period of scheduled "
          "downtime.",
          found->second->description(), found->second->get_hostname());
      _logger->info(
          "SERVICE DOWNTIME ALERT: {};{};STARTED; Service has entered a "
          "period of scheduled downtime",
          found->second->get_hostname(), found->second->description());
      found->second->notify(notifier::reason_downtimestart, author, comment,
                            notifier::notification_option_none);
    }
    found->second->inc_scheduled_downtime_depth();
    found->second->update_status(service::STATUS_DOWNTIME_DEPTH);
  }
}

/**
 * @brief Remove the effect of an ending downtime from a host or service.
 *
 * Decrements the scheduled downtime depth, sends a DOWNTIME STOPPED
 * notification when the depth reaches zero, updates the object status, and
 * decrements the pending flex counter when applicable.
 *
 * @param host_id             The host ID.
 * @param service_id          The service ID, or 0 for a host downtime.
 * @param is_fixed            True if the downtime was fixed.
 * @param incremented_pending True if the pending flex counter was incremented.
 * @param author              The author (for the notification).
 * @param comment             The downtime comment (for the notification).
 */
void engine_downtime_callbacks::end_downtime_effect(
    uint64_t host_id,
    uint64_t service_id,
    bool is_fixed,
    bool incremented_pending,
    const std::string& author,
    const std::string& comment) {
  if (service_id == 0) {
    auto it = host::hosts_by_id.find(host_id);
    if (it == host::hosts_by_id.end() || !it->second)
      return;
    it->second->dec_scheduled_downtime_depth();
    if (it->second->get_scheduled_downtime_depth() == 0) {
      _logger->trace(
          "Host '{}' has exited from a period of scheduled downtime.",
          it->second->name());
      _logger->info(
          "HOST DOWNTIME ALERT: {};STOPPED; Host has exited from a period of "
          "scheduled downtime",
          it->second->name());
      it->second->notify(notifier::reason_downtimeend, author, comment,
                         notifier::notification_option_none);
    }
    it->second->update_status();
    if (!is_fixed && incremented_pending &&
        it->second->get_pending_flex_downtime() > 0)
      it->second->dec_pending_flex_downtime();
  } else {
    auto found = service::services_by_id.find({host_id, service_id});
    if (found == service::services_by_id.end() || !found->second)
      return;
    found->second->dec_scheduled_downtime_depth();
    if (found->second->get_scheduled_downtime_depth() == 0) {
      _logger->trace(
          "Service '{}' on host '{}' has exited from a period of scheduled "
          "downtime.",
          found->second->description(), found->second->get_hostname());
      _logger->info(
          "SERVICE DOWNTIME ALERT: {};{};STOPPED; Service has exited from a "
          "period of scheduled downtime",
          found->second->get_hostname(), found->second->description());
      found->second->notify(notifier::reason_downtimeend, author, comment,
                            notifier::notification_option_none);
    }
    found->second->update_status(service::STATUS_DOWNTIME_DEPTH);
    if (!is_fixed && incremented_pending &&
        found->second->get_pending_flex_downtime() > 0)
      found->second->dec_pending_flex_downtime();
  }
}


/**
 * @brief Create an internal Engine comment associated with a downtime.
 *
 * Called by downtime::subscribe() to attach a human-readable comment to the
 * downtime entry. The comment is stored in Engine's global comment map.
 *
 * @param host_id       The host ID.
 * @param service_id    The service ID, or 0 for a host downtime.
 * @param author        The comment author.
 * @param comment_data  The comment text.
 * @return The unique comment ID assigned by Engine.
 */
uint64_t engine_downtime_callbacks::create_downtime_comment(
    uint64_t host_id,
    uint64_t service_id,
    const std::string& author,
    const std::string& comment_data) {
  comment::type ctype = service_id == 0 ? comment::host : comment::service;
  comment com(ctype, comment::downtime, host_id, service_id, time(nullptr),
              author, comment_data, false, comment::internal, false, (time_t)0);
  return com.get_comment_id();
}

/**
 * @brief Delete the internal Engine comment associated with a downtime.
 *
 * Called by the downtime destructor to clean up the comment created by
 * create_downtime_comment().
 *
 * @param comment_id The comment ID to delete.
 */
void engine_downtime_callbacks::delete_downtime_comment(uint64_t comment_id) {
  comment::delete_comment(comment_id);
}

}  // namespace com::centreon::engine
