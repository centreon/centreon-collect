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
#include "com/centreon/engine/events/loop.hh"
#include "com/centreon/engine/events/timed_event.hh"
#include "com/centreon/engine/host.hh"
#include "com/centreon/engine/service.hh"
#include "common/log_v2/log_v2.hh"

using namespace com::centreon::common::downtimes;

namespace com::centreon::engine {
bool engine_downtime_callbacks::host_exists(uint64_t host_id) const {
  return host::hosts_by_id.contains(host_id);
}

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

std::vector<uint64_t> engine_downtime_callbacks::get_anomaly_detection_services(
    uint64_t host_id,
    uint64_t service_id) const {
  std::vector<uint64_t> result;
  for (const anomalydetection* ano :
       anomalydetection::find_by_dependent_service(host_id, service_id))
    result.push_back(ano->service_id());
  return result;
}

bool engine_downtime_callbacks::cancel_downtime(uint64_t host_id,
                                                uint64_t service_id,
                                                bool is_fixed,
                                                bool incremented_pending,
                                                bool is_in_effect) {
  auto logger = common::log_v2::log_v2::instance().get(
      common::log_v2::log_v2::DOWNTIMES);
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
        logger->info(
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
        logger->info(
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

void engine_downtime_callbacks::schedule_downtime_check(uint64_t downtime_id,
                                                        time_t when) {
  uint64_t* id = new uint64_t{downtime_id};
  events::loop::instance().schedule(
      std::make_unique<timed_event>(timed_event::EVENT_SCHEDULED_DOWNTIME, when,
                                    false, 0, nullptr, false, (void*)id,
                                    nullptr, 0),
      true);
}

void engine_downtime_callbacks::schedule_expire_downtime(time_t when) {
  events::loop::instance().schedule(
      std::make_unique<timed_event>(timed_event::EVENT_EXPIRE_DOWNTIME, when,
                                    false, 0, nullptr, false, nullptr, nullptr,
                                    0),
      true);
}

void engine_downtime_callbacks::remove_downtime_check(uint64_t downtime_id) {
  events::loop::instance().remove_downtime(downtime_id);
}

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
                                              uint64_t downtime_id) const {
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

bool engine_downtime_callbacks::object_exists(uint64_t host_id,
                                              uint64_t service_id) const {
  return service_id == 0 ? host_exists(host_id)
                         : service_exists(host_id, service_id);
}

bool engine_downtime_callbacks::is_object_ok(uint64_t host_id,
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

void engine_downtime_callbacks::start_downtime_effect(
    uint64_t host_id, uint64_t service_id, const std::string& author,
    const std::string& comment) {
  auto logger = common::log_v2::log_v2::instance().get(
      common::log_v2::log_v2::DOWNTIMES);
  if (service_id == 0) {
    auto it = host::hosts_by_id.find(host_id);
    if (it == host::hosts_by_id.end() || !it->second)
      return;
    if (it->second->get_scheduled_downtime_depth() == 0) {
      logger->trace("Host '{}' has entered a period of scheduled downtime.",
                    it->second->name());
      logger->info(
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
      logger->trace(
          "Service '{}' on host '{}' has entered a period of scheduled "
          "downtime.",
          found->second->description(), found->second->get_hostname());
      logger->info(
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

void engine_downtime_callbacks::end_downtime_effect(
    uint64_t host_id, uint64_t service_id, bool is_fixed,
    bool incremented_pending, const std::string& author,
    const std::string& comment) {
  auto logger = common::log_v2::log_v2::instance().get(
      common::log_v2::log_v2::DOWNTIMES);
  if (service_id == 0) {
    auto it = host::hosts_by_id.find(host_id);
    if (it == host::hosts_by_id.end() || !it->second)
      return;
    it->second->dec_scheduled_downtime_depth();
    if (it->second->get_scheduled_downtime_depth() == 0) {
      logger->trace(
          "Host '{}' has exited from a period of scheduled downtime.",
          it->second->name());
      logger->info(
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
      logger->trace(
          "Service '{}' on host '{}' has exited from a period of scheduled "
          "downtime.",
          found->second->description(), found->second->get_hostname());
      logger->info(
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



}  // namespace com::centreon::engine
