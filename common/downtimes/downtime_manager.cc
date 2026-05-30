/**
 * Copyright 2019-2026 Centreon (https://www.centreon.com/)
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

#include "common/downtimes/downtime_manager.hh"

using com::centreon::exceptions::msg_fmt;

namespace com::centreon::common::downtimes {

std::unique_ptr<downtime_manager> downtime_manager::_instance = nullptr;

/**
 * @brief Constructs the downtime manager with the given callbacks.
 *
 * @param callbacks Engine-specific callbacks for host/service existence checks
 * and anomaly detection queries. Ownership is transferred.
 */
downtime_manager::downtime_manager(
    std::unique_ptr<downtime_callbacks> callbacks)
    : _callbacks(std::move(callbacks)) {}

/**
 *  Remove a service/host downtime from its id.
 *
 * @param downtime_id The downtime's id
 *
 */
void downtime_manager::delete_downtime(uint64_t downtime_id) {
  SPDLOG_LOGGER_TRACE(_logger, "delete_downtime({})", downtime_id);
  auto it =
      std::find_if(_scheduled_downtimes.begin(), _scheduled_downtimes.end(),
                   [downtime_id](const auto& p) {
                     return p.second->get_downtime_id() == downtime_id;
                   });
  if (it != _scheduled_downtimes.end()) {
    SPDLOG_LOGGER_TRACE(_logger, "delete downtime(id: {})", downtime_id);
    _scheduled_downtimes.erase(it);
  }
}

/**
 * @brief Unschedule a host or service downtime.
 *
 * @param downtime_id The downtime's id
 *
 * @return true on success, false if the downtime was not found or could not be
 * unscheduled.
 */
bool downtime_manager::unschedule_downtime(uint64_t downtime_id) {
  auto found = std::find_if(
      _scheduled_downtimes.begin(), _scheduled_downtimes.end(),
      [&downtime_id](std::pair<time_t, std::shared_ptr<downtime>> d) {
        return downtime_id == d.second->get_downtime_id();
      });

  SPDLOG_LOGGER_TRACE(_logger, "unschedule downtime(id: {})", downtime_id);

  /* find the downtime entry in the list in memory */
  if (found == _scheduled_downtimes.end()) {
    SPDLOG_LOGGER_DEBUG(_logger, "unknown downtime(id: {})", downtime_id);
    return false;
  }

  if (!found->second->unschedule())
    return false;

  /* remove scheduled entry from event queue */
  _callbacks->remove_downtime_check(downtime_id);

  /* delete downtime entry */
  _scheduled_downtimes.erase(found);

  /* unschedule all downtime entries that were triggered by this one */
  std::list<uint64_t> lst;
  for (auto it = _scheduled_downtimes.begin(), end = _scheduled_downtimes.end();
       it != end; ++it) {
    if (it->second->get_triggered_by() == downtime_id)
      lst.push_back(it->second->get_downtime_id());
  }

  for (uint64_t id : lst) {
    SPDLOG_LOGGER_TRACE(_logger, "Unschedule triggered downtime (id: {})", id);
    unschedule_downtime(id);
  }
  return true;
}

/**
 * @brief Find a specific downtime entry by type and id.
 *
 * @param type The downtime type to search for (host_downtime, service_downtime,
 * or any_downtime).
 * @param downtime_id The unique identifier of the downtime to find.
 *
 * @return A shared pointer to the downtime if found, or nullptr if no matching
 * downtime is found.
 */
std::shared_ptr<downtime> downtime_manager::find_downtime(
    downtime::type type,
    uint64_t downtime_id) {
  for (std::multimap<time_t, std::shared_ptr<downtime>>::iterator
           it = _scheduled_downtimes.begin(),
           end = _scheduled_downtimes.end();
       it != end; ++it) {
    if (type != downtime::any_downtime && it->second->get_type() != type)
      continue;
    if (it->second->get_downtime_id() == downtime_id)
      return it->second;
  }
  return nullptr;
}

/**
 * @brief Activates pending flexible host downtimes for the given host.
 *
 * The caller is responsible for ensuring the host is not in UP state.
 *
 * @param host_id The host for which to activate flexible downtimes.
 */
void downtime_manager::activate_pending_flex_host_downtimes(uint64_t host_id) {
  SPDLOG_LOGGER_TRACE(_logger, "activate_pending_flex_host_downtimes({})",
                      host_id);

  time_t current_time = time(nullptr);

  for (auto& [_, dt] : _scheduled_downtimes) {
    if (dt->get_type() != downtime::host_downtime || dt->is_fixed() ||
        dt->is_in_effect() || dt->get_triggered_by() != 0 ||
        dt->host_id() != host_id)
      continue;

    if (dt->get_start_time() <= current_time &&
        current_time <= dt->get_end_time()) {
      SPDLOG_LOGGER_TRACE(_logger,
                          "Flexible downtime (id={}) for host {} starting now.",
                          dt->get_downtime_id(), host_id);
      dt->start_flex_downtime();
      dt->handle();
    }
  }
}

/**
 * @brief Activates pending flexible service downtimes for the given service.
 *
 * The caller is responsible for ensuring the service is not in OK state.
 *
 * @param host_id    The host owning the service.
 * @param service_id The service for which to activate flexible downtimes.
 */
void downtime_manager::activate_pending_flex_service_downtimes(
    uint64_t host_id,
    uint64_t service_id) {
  SPDLOG_LOGGER_TRACE(_logger,
                      "activate_pending_flex_service_downtimes({}, {})",
                      host_id, service_id);

  time_t current_time = time(nullptr);

  for (auto& [_, dt] : _scheduled_downtimes) {
    if (dt->get_type() != downtime::service_downtime || dt->is_fixed() ||
        dt->is_in_effect() || dt->get_triggered_by() != 0 ||
        dt->host_id() != host_id)
      continue;

    if (dt->service_id() != service_id)
      continue;

    if (dt->get_start_time() <= current_time &&
        current_time <= dt->get_end_time()) {
      SPDLOG_LOGGER_TRACE(
          _logger,
          "Flexible downtime (id={}) for service ({}, {}) starting now.",
          dt->get_downtime_id(), host_id, service_id);
      dt->start_flex_downtime();
      dt->handle();
    }
  }
}

/**
 * @brief Returns a read-only reference to the scheduled downtimes map.
 *
 * @return The internal multimap of scheduled downtimes, keyed by start time.
 */
std::multimap<time_t, std::shared_ptr<downtime>> const&
downtime_manager::get_scheduled_downtimes() const {
  return _scheduled_downtimes;
}

/** @brief Removes all scheduled downtimes from the internal map. */
void downtime_manager::clear_scheduled_downtimes() {
  _scheduled_downtimes.clear();
}

/**
 * @brief Inserts a downtime into the internal scheduled downtimes map.
 *
 * @param dt The downtime to insert, keyed by its start time.
 */
void downtime_manager::add_downtime(
    const std::shared_ptr<downtime>& dt) noexcept {
  _scheduled_downtimes.insert({dt->get_start_time(), dt});
}

/**
 * @brief Deletes all expired downtimes from the scheduled downtimes list. A
 * downtime is considered expired if it is not currently in effect and its end
 * time is in the past.
 */
void downtime_manager::delete_expired_downtimes() {
  SPDLOG_LOGGER_TRACE(_logger, "delete_expired_downtimes()");

  time_t current_time = time(nullptr);

  auto next_it = _scheduled_downtimes.begin();
  for (auto it = _scheduled_downtimes.begin(), end = _scheduled_downtimes.end();
       it != end; it = next_it) {
    downtime& dt(*it->second);
    ++next_it;

    if (!dt.is_in_effect() && dt.get_end_time() < current_time) {
      SPDLOG_LOGGER_TRACE(
          _logger, "Expiring {} downtime (id={})...",
          dt.get_type() == downtime::host_downtime ? "host" : "service",
          dt.get_downtime_id());
      delete_downtime(dt.get_downtime_id());
    }
  }
}

/**
 * @brief Deletes all host and service downtimes on a host by hostname,
 * optionally filtered by service description, start time and comment. All
 * string parameters must be set or empty - an empty string will silently fail
 * to match. Returns the number of downtimes deleted.
 *
 * @param hostname The name of the host for which to delete downtimes. If empty,
 * downtimes for all hosts will be considered.
 * @param service_description The description of the service for which to delete
 * downtimes. If empty, downtimes for all services will be considered.
 * @param start_time Optional start time to filter by. If std::nullopt,
 * downtimes will not be filtered by start time.
 * @param comment The comment associated with the downtime to delete. If empty,
 * downtimes with any comment will be considered.
 *
 * @return The number of downtimes that were deleted.
 */
int downtime_manager::
    delete_downtime_by_hostname_service_description_start_time_comment(
        const std::string& hostname,
        const std::string& service_description,
        std::optional<time_t> start_time,
        const std::string& comment) {
  SPDLOG_LOGGER_TRACE(
      _logger,
      "Delete downtimes (host: '{}', service description: '{}', start time: "
      "{}, comment: '{}')",
      hostname, service_description, start_time.value_or(0), comment);
  int deleted{0};

  /* Do not allow deletion of everything - must have at least 1 filter on. */
  if (hostname.empty() && service_description.empty() && !start_time &&
      comment.empty())
    return deleted;

  std::pair<std::multimap<time_t, std::shared_ptr<downtime>>::iterator,
            std::multimap<time_t, std::shared_ptr<downtime>>::iterator>
      range;

  if (start_time)
    range = _scheduled_downtimes.equal_range(*start_time);
  else
    range = {_scheduled_downtimes.begin(), _scheduled_downtimes.end()};

  std::list<uint64_t> lst;
  for (auto it = range.first, end = range.second; it != end; ++it) {
    if (!comment.empty() && it->second->get_comment() != comment)
      continue;
    if (downtime::host_downtime == it->second->get_type()) {
      std::string name = downtime_manager::instance().callbacks().get_host_name(
          it->second->host_id());
      /* If service is specified, then do not delete the host downtime. */
      if (!service_description.empty())
        continue;
      if (!hostname.empty() && name != hostname)
        continue;
    } else if (downtime::service_downtime == it->second->get_type()) {
      auto p = _callbacks->get_host_and_service_names(it->second->host_id(),
                                                      it->second->service_id());
      if (!hostname.empty() && p.first != hostname)
        continue;

      if (p.second != service_description)
        continue;
    }
    lst.push_back(it->second->get_downtime_id());
    ++deleted;
  }

  for (auto id : lst)
    unschedule_downtime(id);

  return deleted;
}

/**
 * @brief Initializes downtime data: removes stale entries and resets the ID
 * counter so the next call to get_next_downtime_id() rescans.
 */
void downtime_manager::initialize_downtime_data() {
  SPDLOG_LOGGER_TRACE(_logger, "downtime_manager::initialize_downtime_data()");
  /* clean up the old downtime data */
  validate_downtime_data();

  _next_id = 0;
}

/**
 * @brief Removes invalid downtime entries from the scheduled downtimes list.
 *
 * Two passes are performed:
 * - First, stale downtimes (invalid host/service or expired) are removed.
 * - Then, triggered downtimes whose parent no longer exists are removed.
 */
void downtime_manager::validate_downtime_data() {
  /* remove stale downtimes */
  for (auto it = _scheduled_downtimes.begin();
       it != _scheduled_downtimes.end();) {
    if (it->second->is_stale())
      it = _scheduled_downtimes.erase(it);
    else
      ++it;
  }

  /* remove triggered downtimes without valid parents */
  for (auto it = _scheduled_downtimes.begin();
       it != _scheduled_downtimes.end();) {
    if (it->second->get_triggered_by() &&
        !find_downtime(downtime::any_downtime, it->second->get_triggered_by()))
      it = _scheduled_downtimes.erase(it);
    else
      ++it;
  }
}

/**
 *  Return the next downtime id to use.
 *
 * @return an id as an unsigned long.
 */
uint64_t downtime_manager::get_next_downtime_id() {
  if (_next_id == 0) {
    for (const auto& [_, dt] : _scheduled_downtimes)
      if (dt->get_downtime_id() > _next_id)
        _next_id = dt->get_downtime_id();
  }
  return ++_next_id;
}

/**
 * @brief Creates and schedules a new downtime (host or service).
 *
 * @param host_id      The host to put in downtime.
 * @param service_id   The service to put in downtime (0 for host downtime).
 * @param entry_time   Time the downtime was entered.
 * @param author       Author of the downtime.
 * @param comment_data Comment associated with the downtime.
 * @param start_time   Scheduled start time.
 * @param end_time     Scheduled end time.
 * @param fixed        True for a fixed downtime, false for flexible.
 * @param triggered_by ID of the parent downtime (0 if none).
 * @param duration     Duration in seconds (for flexible downtimes).
 *
 * @return The newly created downtime.
 * @throws msg_fmt if the host or service does not exist.
 */
std::shared_ptr<downtime> downtime_manager::add_new_downtime(
    uint64_t host_id,
    uint64_t service_id,
    time_t entry_time,
    const std::string& author,
    const std::string& comment_data,
    time_t start_time,
    time_t end_time,
    bool fixed,
    uint64_t triggered_by,
    uint32_t duration) {
  if (service_id == 0) {
    if (!_callbacks->host_exists(host_id))
      throw msg_fmt(
          "can not create a host downtime on host {} because it does not exist",
          host_id);
  } else {
    if (!_callbacks->service_exists(host_id, service_id))
      throw msg_fmt(
          "can not create a service downtime on service ({}, {}) which does "
          "not exist",
          host_id, service_id);
  }

  uint64_t new_downtime_id = get_next_downtime_id();

  auto retval = std::make_shared<downtime>(host_id, service_id, entry_time,
                                           author, comment_data, start_time,
                                           end_time, fixed, triggered_by,
                                           duration, new_downtime_id, _logger);
  add_downtime(retval);

  _callbacks->notify_broker(downtime_callbacks::ADD,
                            downtime_callbacks::ATTR_NONE, host_id, service_id,
                            author, comment_data, entry_time, start_time,
                            end_time, fixed, triggered_by, duration,
                            new_downtime_id);
  return retval;
}

/**
 * @brief Validates parameters and schedules a new host or service downtime.
 *
 * Clamps start_time, end_time and duration to sane limits, creates the
 * downtime object, and registers it. For service downtimes, any associated
 * anomaly-detection service is also put in downtime, triggered by this one.
 *
 * @param type            downtime::host_downtime or downtime::service_downtime.
 * @param host_id         Host to put in downtime.
 * @param service_id      Service to put in downtime (ignored for host
 * downtime).
 * @param entry_time      Time the downtime was entered.
 * @param author          Author of the downtime.
 * @param comment_data    Comment associated with the downtime.
 * @param start_time      Scheduled start time (clamped to 2100-01-01 max).
 * @param end_time        Scheduled end time (clamped to 2100-01-01 max).
 * @param fixed           True for a fixed downtime, false for flexible.
 * @param triggered_by    ID of the parent downtime (0 if none).
 * @param duration        Duration in seconds for flexible downtimes (clamped to
 * 366 days max).
 * @param new_downtime_id Output parameter filled with the new downtime ID, or
 * nullptr.
 *
 * @return true on success, false if the time range is invalid or already past.
 */
bool downtime_manager::schedule_downtime(downtime::type type,
                                         const uint64_t host_id,
                                         const uint64_t service_id,
                                         time_t entry_time,
                                         const std::string& author,
                                         const std::string& comment_data,
                                         time_t start_time,
                                         time_t end_time,
                                         bool fixed,
                                         uint64_t triggered_by,
                                         uint32_t duration,
                                         uint64_t* new_downtime_id) {
  SPDLOG_LOGGER_TRACE(_logger, "schedule_downtime()");

  /* don't add old or invalid downtimes */
  if (start_time >= end_time || end_time <= time(nullptr))
    return false;

  if (start_time > 4102441200) {
    _logger->warn(
        "SCHEDULE DOWNTIME ALERT : start time is out of range and setted to "
        "1/1/2100 00:00");
    start_time = 4102441200;
  }

  if (end_time > 4102441200) {
    _logger->warn(
        "SCHEDULE DOWNTIME ALERT : end time is out of range and setted to "
        "1/1/2100 00:00");
    end_time = 4102441200;
  }

  if (duration > 31622400) {
    _logger->warn(
        "SCHEDULE DOWNTIME ALERT : is too long and setted to 366 days");
    duration = 31622400;
  }

  /* For host downtime, service_id is 0; for service downtime, use service_id */
  uint64_t effective_service_id =
      (type == downtime::host_downtime) ? 0 : service_id;

  std::shared_ptr<downtime> dt;
  dt = add_new_downtime(host_id, effective_service_id, entry_time, author,
                        comment_data, start_time, end_time, fixed, triggered_by,
                        duration);

  if (type == downtime::service_downtime) {
    for (uint64_t ano_sid :
         _callbacks->get_anomaly_detection_services(host_id, service_id)) {
      uint64_t ano_downtime_id;
      schedule_downtime(downtime::service_downtime, host_id, ano_sid,
                        entry_time, author, comment_data, start_time, end_time,
                        fixed, dt->get_downtime_id(), duration,
                        &ano_downtime_id);
    }
  }

  if (new_downtime_id)
    *new_downtime_id = dt->get_downtime_id();

  if (!dt->subscribe())
    return false;

  return true;
}

/** @brief Returns a reference to the engine-specific downtime callbacks. */
downtime_callbacks& downtime_manager::callbacks() const {
  return *_callbacks;
}

}  // namespace com::centreon::common::downtimes
