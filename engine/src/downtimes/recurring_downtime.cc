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

#include "com/centreon/engine/downtimes/recurring_downtime.hh"

#include "com/centreon/engine/downtimes/downtime_manager.hh"
#include "com/centreon/engine/downtimes/service_downtime.hh"
#include "com/centreon/engine/globals.hh"
#include "com/centreon/engine/host.hh"
#include "com/centreon/engine/service.hh"
#include "com/centreon/engine/timezone_locker.hh"

using namespace com::centreon::engine;
using namespace com::centreon::engine::downtimes;

std::map<uint64_t, std::shared_ptr<recurring_downtime>>
    recurring_downtime::recurring_downtimes;
bool recurring_downtime::_initial_load_done = false;

namespace {
/**
 * @brief Local time of a day plus some minutes, DST resolved by mktime.
 */
time_t local_time(const struct tm& day, uint32_t minutes) {
  struct tm t {};
  t.tm_year = day.tm_year;
  t.tm_mon = day.tm_mon;
  t.tm_mday = day.tm_mday;
  t.tm_hour = minutes / 60;
  t.tm_min = minutes % 60;
  t.tm_isdst = -1;
  return mktime(&t);
}

bool contains(const google::protobuf::RepeatedField<uint32_t>& days,
              uint32_t day) {
  return std::find(days.begin(), days.end(), day) != days.end();
}

/**
 * @brief Whether a period applies to a local day (normalized by mktime).
 */
bool period_matches(const configuration::RecurringdowntimePeriod& p,
                    const struct tm& day) {
  const uint32_t weekday = day.tm_wday == 0 ? 7 : day.tm_wday;
  switch (p.kind()) {
    case configuration::RecurringdowntimePeriod::weekly:
      return contains(p.days(), weekday);
    case configuration::RecurringdowntimePeriod::monthly:
      return contains(p.days(), day.tm_mday);
    case configuration::RecurringdowntimePeriod::nth: {
      if (p.days().empty() || p.days(0) != weekday)
        return false;
      if (p.rank() == -1) {
        struct tm next = day;
        next.tm_mday += 7;
        next.tm_hour = 12;
        next.tm_isdst = -1;
        mktime(&next);
        return next.tm_mon != day.tm_mon;
      }
      return (day.tm_mday - 1) / 7 + 1 == p.rank();
    }
    default:
      return false;
  }
}
}  // namespace

std::vector<occurrence> downtimes::compute_occurrences(
    const configuration::Recurringdowntime& rule,
    const std::string& tz,
    time_t now,
    time_t look_ahead) {
  std::vector<occurrence> retval;
  timezone_locker lock(tz);

  struct tm today;
  localtime_r(&now, &today);
  for (int offset = -1; offset <= 1; ++offset) {
    // Noon is never skipped by a DST change.
    struct tm day {};
    day.tm_year = today.tm_year;
    day.tm_mon = today.tm_mon;
    day.tm_mday = today.tm_mday + offset;
    day.tm_hour = 12;
    day.tm_isdst = -1;
    mktime(&day);

    for (const auto& p : rule.periods()) {
      if (!period_matches(p, day))
        continue;
      occurrence occ{local_time(day, p.start_minute()),
                     local_time(day, p.end_minute()), p.fixed(),
                     p.duration()};
      // A window that DST normalisation makes empty does not exist that day.
      if (occ.start >= occ.end)
        continue;
      if (occ.start <= now + look_ahead && occ.end > now)
        retval.push_back(occ);
    }
  }
  std::sort(retval.begin(), retval.end(),
            [](const occurrence& a, const occurrence& b) {
              return a.start < b.start || (a.start == b.start && a.end < b.end);
            });
  retval.erase(std::unique(retval.begin(), retval.end()), retval.end());
  return retval;
}

/**
 * @brief Constructor.
 *
 * @param config The configuration of the rule, copied.
 * @param not_before Occurrences starting before this time are not scheduled
 * (see not_before_for_new_rule()).
 */
recurring_downtime::recurring_downtime(
    const configuration::Recurringdowntime& config,
    time_t not_before)
    : _config(config), _not_before(not_before) {}

/**
 * @brief The `not_before` of a rule created now: the process start time during
 * the first configuration load, 0 (no limit) once it is done.
 */
time_t recurring_downtime::not_before_for_new_rule() {
  return _initial_load_done ? 0 : program_start;
}

/**
 * @brief Called at the end of each configuration apply: rules created later
 * come from a reload.
 */
void recurring_downtime::set_initial_load_done() {
  _initial_load_done = true;
}

/**
 * @brief Forget every rule, as if no configuration had been loaded.
 */
void recurring_downtime::clear() {
  recurring_downtimes.clear();
  _initial_load_done = false;
}

/**
 * @brief Set the targets of the rule known to this Engine.
 *
 * @param host_ids Hosts put in downtime with all their services.
 * @param service_ids Services put in downtime.
 */
void recurring_downtime::set_targets(
    std::vector<uint64_t>&& host_ids,
    std::vector<std::pair<uint64_t, uint64_t>>&& service_ids) {
  _host_ids = std::move(host_ids);
  _service_ids = std::move(service_ids);
}

/**
 * @brief The comment of the downtimes of a rule, the same as the one the
 * Centreon Web cron used.
 */
std::string recurring_downtime::comment(uint64_t downtime_id) {
  return fmt::format("[Downtime cycle #{}]", downtime_id);
}

/**
 * @brief Schedule one occurrence on one host (service_id 0) or service, at
 * most once per rule object, unless it starts before _not_before or a downtime
 * with the same target, comment and start time already exists.
 */
void recurring_downtime::_schedule_one(uint64_t host_id,
                                       uint64_t service_id,
                                       const occurrence& occ,
                                       time_t now) {
  // Handled once: a downtime cancelled since then is not created again.
  if (!_handled.emplace(std::make_tuple(occ.start, host_id, service_id), occ.end)
           .second)
    return;
  if (occ.start < _not_before) {
    SPDLOG_LOGGER_DEBUG(downtimes_logger,
                        "Recurring downtime #{}: occurrence of {}/{} at {} "
                        "started before Engine: not scheduled",
                        id(), host_id, service_id, occ.start);
    return;
  }

  // SCHEDULE_*_DOWNTIME refuses a flexible downtime without duration: so does
  // the rule (a warning is logged at apply).
  if (!occ.fixed && occ.duration == 0) {
    SPDLOG_LOGGER_DEBUG(downtimes_logger,
                        "Recurring downtime #{}: flexible occurrence at {} "
                        "without duration skipped",
                        id(), occ.start);
    return;
  }
  const downtime::type type =
      service_id ? downtime::service_downtime : downtime::host_downtime;
  const std::string cmt = comment(id());
  downtime_manager& mgr = downtime_manager::instance();

  auto range = mgr.get_scheduled_downtimes().equal_range(occ.start);
  for (auto it = range.first; it != range.second; ++it) {
    const downtime& dt = *it->second;
    if (dt.get_type() != type || dt.host_id() != host_id ||
        dt.get_comment() != cmt)
      continue;
    if (type == downtime::service_downtime &&
        static_cast<const service_downtime&>(dt).service_id() != service_id)
      continue;
    SPDLOG_LOGGER_DEBUG(downtimes_logger,
                        "Recurring downtime #{}: downtime {} already scheduled "
                        "on {}/{} at {}",
                        id(), dt.get_downtime_id(), host_id, service_id,
                        occ.start);
    return;
  }

  unsigned long duration = occ.fixed ? occ.end - occ.start : occ.duration;
  uint64_t new_id = 0;
  if (mgr.schedule_downtime(type, host_id, service_id, now, author,
                            cmt.c_str(), occ.start, occ.end, occ.fixed, 0,
                            duration, &new_id) == OK) {
    if (type == downtime::host_downtime)
      SPDLOG_LOGGER_INFO(downtimes_logger,
                         "Recurring downtime #{}: host downtime {} scheduled "
                         "on host {} from {} to {}",
                         id(), new_id, host_id, occ.start, occ.end);
    else
      SPDLOG_LOGGER_INFO(downtimes_logger,
                         "Recurring downtime #{}: service downtime {} "
                         "scheduled on service {}/{} from {} to {}",
                         id(), new_id, host_id, service_id, occ.start, occ.end);
  } else
    SPDLOG_LOGGER_WARN(downtimes_logger,
                       "Recurring downtime #{}: could not schedule the "
                       "downtime of {}/{} from {} to {}",
                       id(), host_id, service_id, occ.start, occ.end);
}

/**
 * @brief Schedule the occurrences of the rule that start within 10 minutes or
 * are running, on each of its targets.
 *
 * @param now The current time.
 */
void recurring_downtime::schedule(time_t now) {
  for (auto it = _handled.begin(); it != _handled.end();) {
    if (it->second <= now)
      it = _handled.erase(it);
    else
      ++it;
  }

  // Occurrences depend only on the timezone: compute them once per timezone.
  absl::flat_hash_map<std::string, std::vector<occurrence>> by_tz;
  auto occurrences = [&](const std::string& host_tz)
      -> const std::vector<occurrence>& {
    const std::string& tz =
        _config.timezone().empty() ? host_tz : _config.timezone();
    auto it = by_tz.find(tz);
    if (it == by_tz.end())
      it = by_tz.emplace(tz, compute_occurrences(_config, tz, now)).first;
    return it->second;
  };
  static const std::string no_tz;

  for (uint64_t host_id : _host_ids) {
    auto hst = host::hosts_by_id.find(host_id);
    if (hst == host::hosts_by_id.end() || !hst->second)
      continue;
    const auto& occs = occurrences(hst->second->get_timezone());
    SPDLOG_LOGGER_DEBUG(downtimes_logger,
                        "Recurring downtime #{}: {} occurrence(s) for host {}",
                        id(), occs.size(), host_id);
    for (const occurrence& occ : occs) {
      _schedule_one(host_id, 0, occ, now);
      for (const auto& p : hst->second->services)
        if (p.second)
          _schedule_one(host_id, p.second->service_id(), occ, now);
    }
  }

  for (const auto& [host_id, service_id] : _service_ids) {
    if (service::services_by_id.find({host_id, service_id}) ==
        service::services_by_id.end())
      continue;
    auto hst = host::hosts_by_id.find(host_id);
    const auto& occs =
        occurrences(hst != host::hosts_by_id.end() && hst->second
                        ? hst->second->get_timezone()
                        : no_tz);
    SPDLOG_LOGGER_DEBUG(
        downtimes_logger,
        "Recurring downtime #{}: {} occurrence(s) for service {}/{}", id(),
        occs.size(), host_id, service_id);
    for (const occurrence& occ : occs)
      _schedule_one(host_id, service_id, occ, now);
  }
}

/**
 * @brief Run every rule. Called by the recurring timed event and after each
 * configuration apply, on the main loop.
 *
 * @param now The current time.
 */
void recurring_downtime::schedule_all(time_t now) {
  for (const auto& [rule_id, rule] : recurring_downtimes) {
    try {
      rule->schedule(now);
    } catch (const std::exception& e) {
      SPDLOG_LOGGER_ERROR(downtimes_logger,
                          "Recurring downtime #{}: scheduling failed: {}",
                          rule_id, e.what());
    }
  }
}

/**
 * @brief Delete the downtimes of a rule that have not started yet; running
 * ones go on.
 *
 * @param downtime_id The id of the rule.
 * @param now The current time.
 *
 * @return The number of deleted downtimes.
 */
int recurring_downtime::delete_future_downtimes(uint64_t downtime_id,
                                                time_t now) {
  const std::string cmt = comment(downtime_id);
  downtime_manager& mgr = downtime_manager::instance();
  std::vector<uint64_t> ids;
  for (const auto& [start, dt] : mgr.get_scheduled_downtimes())
    if (start > now && !dt->is_in_effect() && dt->get_comment() == cmt)
      ids.push_back(dt->get_downtime_id());

  int deleted = 0;
  for (uint64_t id : ids)
    if (mgr.unschedule_downtime(id) == OK)
      ++deleted;
  if (deleted)
    SPDLOG_LOGGER_INFO(downtimes_logger,
                       "Recurring downtime #{}: {} future downtime(s) deleted",
                       downtime_id, deleted);
  return deleted;
}
