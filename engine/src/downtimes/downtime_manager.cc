/**
 * Copyright 2019-2024 Centreon (https://www.centreon.com/)
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

#include "com/centreon/engine/downtimes/downtime_manager.hh"

#include "com/centreon/engine/anomalydetection.hh"
#include "com/centreon/engine/broker.hh"
#include "com/centreon/engine/configuration/applier/state.hh"
#include "com/centreon/engine/downtimes/host_downtime.hh"
#include "com/centreon/engine/downtimes/service_downtime.hh"
#include "com/centreon/engine/events/loop.hh"
#include "com/centreon/engine/exceptions/error.hh"
#include "com/centreon/engine/globals.hh"
#include "com/centreon/engine/logging/logger.hh"

using namespace com::centreon::engine;
using namespace com::centreon::engine::configuration::applier;
using namespace com::centreon::engine::downtimes;
using namespace com::centreon::engine::logging;

/**
 *  Remove a service/host downtime from its id.
 *
 * @param type downtime::host_downtime or downtime::service_downtime
 * @param downtime_id The downtime's id
 *
 */
void downtime_manager::delete_downtime(uint64_t downtime_id) {
  SPDLOG_LOGGER_TRACE(functions_logger, "delete_downtime({})", downtime_id);
  /* find the downtime we should remove */
  for (auto it = _scheduled_downtimes.begin(), end = _scheduled_downtimes.end();
       it != end; ++it) {
    if (it->second->get_downtime_id() == downtime_id) {
      engine_logger(dbg_downtime, basic)
          << "delete downtime(id: " << downtime_id << ")";
      SPDLOG_LOGGER_TRACE(downtimes_logger, "delete downtime(id: {})",
                          downtime_id);
      _scheduled_downtimes.erase(it);
      break;
    }
  }
}

/* unschedules a host or service downtime */
int downtime_manager::unschedule_downtime(uint64_t downtime_id) {
  auto found = std::find_if(
      _scheduled_downtimes.begin(), _scheduled_downtimes.end(),
      [&downtime_id](std::pair<time_t, std::shared_ptr<downtime>> d) {
        return downtime_id == d.second->get_downtime_id();
      });

  engine_logger(dbg_functions, basic) << "unschedule_downtime()";
  SPDLOG_LOGGER_TRACE(functions_logger, "unschedule_downtime()");
  engine_logger(dbg_downtime, basic)
      << "unschedule downtime(id: " << downtime_id << ")";
  SPDLOG_LOGGER_TRACE(downtimes_logger, "unschedule downtime(id: {})",
                      downtime_id);

  /* find the downtime entry in the list in memory */
  if (found == _scheduled_downtimes.end()) {
    SPDLOG_LOGGER_DEBUG(downtimes_logger, "unknown downtime(id: {})",
                        downtime_id);
    return ERROR;
  }

  if (found->second->unschedule() == ERROR)
    return ERROR;

  /* remove scheduled entry from event queue */
  events::loop::instance().remove_downtime(downtime_id);

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
    engine_logger(dbg_downtime, basic)
        << "Unschedule triggered downtime (id: " << id << ")";
    SPDLOG_LOGGER_TRACE(downtimes_logger,
                        "Unschedule triggered downtime (id: {})", id);
    unschedule_downtime(id);
  }
  return OK;
}

/* finds a specific downtime entry */
std::shared_ptr<downtime> downtime_manager::find_downtime(
    downtime::type type,
    uint64_t downtime_id) {
  for (std::multimap<time_t, std::shared_ptr<downtime>>::iterator
           it{_scheduled_downtimes.begin()},
       end{_scheduled_downtimes.end()};
       it != end; ++it) {
    if (type != downtime::any_downtime && it->second->get_type() != type)
      continue;
    if (it->second->get_downtime_id() == downtime_id)
      return it->second;
  }
  return nullptr;
}

/* checks for flexible (non-fixed) host downtime that should start now */
int downtime_manager::check_pending_flex_host_downtime(host* hst) {
  time_t current_time(0L);

  engine_logger(dbg_functions, basic) << "check_pending_flex_host_downtime()";
  SPDLOG_LOGGER_TRACE(functions_logger, "check_pending_flex_host_downtime()");

  if (hst == nullptr)
    return ERROR;

  time(&current_time);

  /* if host is currently up, nothing to do */
  if (hst->get_current_state() == host::state_up)
    return OK;

  /* check all downtime entries */
  for (std::multimap<time_t, std::shared_ptr<downtime>>::iterator
           it{_scheduled_downtimes.begin()},
       end{_scheduled_downtimes.end()};
       it != end; ++it) {
    if (it->second->get_type() != downtime::host_downtime ||
        it->second->is_fixed() || it->second->is_in_effect() ||
        it->second->get_triggered_by() != 0)
      continue;

    /* this entry matches our host! */
    host* temp_host(nullptr);
    host_id_map::const_iterator it_hg =
        host::hosts_by_id.find(it->second->host_id());
    if (it_hg != host::hosts_by_id.end())
      temp_host = it_hg->second.get();

    if (temp_host == hst) {
      /* if the time boundaries are okay, start this scheduled downtime */
      if (it->second->get_start_time() <= current_time &&
          current_time <= it->second->get_end_time()) {
        engine_logger(dbg_downtime, basic)
            << "Flexible downtime (id=" << it->second->get_downtime_id()
            << ") for host '" << hst->name() << "' starting now...";
        SPDLOG_LOGGER_TRACE(
            downtimes_logger,
            "Flexible downtime (id={}) for host '{}' starting now...",
            it->second->get_downtime_id(), hst->name());

        it->second->start_flex_downtime();
        it->second->handle();
      }
    }
  }
  return OK;
}

/* checks for flexible (non-fixed) service downtime that should start now */
int downtime_manager::check_pending_flex_service_downtime(service* svc) {
  time_t current_time(0L);

  engine_logger(dbg_functions, basic)
      << "check_pending_flex_service_downtime()";
  SPDLOG_LOGGER_TRACE(functions_logger,
                      "check_pending_flex_service_downtime()");

  if (svc == nullptr)
    return ERROR;

  time(&current_time);

  /* if service is currently ok, nothing to do */
  if (svc->get_current_state() == service::state_ok)
    return OK;

  /* check all downtime entries */
  for (std::multimap<time_t, std::shared_ptr<downtime>>::iterator
           it{_scheduled_downtimes.begin()},
       end{_scheduled_downtimes.end()};
       it != end; ++it) {
    if (it->second->get_type() != downtime::service_downtime ||
        it->second->is_fixed() || it->second->is_in_effect() ||
        it->second->get_triggered_by() != 0)
      continue;

    service_downtime& dt(
        *std::static_pointer_cast<service_downtime>(it->second));

    auto found = service::services_by_id.find(
        std::make_pair(dt.host_id(), dt.service_id()));

    /* this entry matches our service! */
    if (found != service::services_by_id.end() && found->second.get() == svc) {
      /* if the time boundaries are okay, start this scheduled downtime */
      if (dt.get_start_time() <= current_time &&
          current_time <= dt.get_end_time()) {
        engine_logger(dbg_downtime, basic)
            << "Flexible downtime (id=" << dt.get_downtime_id()
            << ") for service '" << svc->description() << "' on host '"
            << svc->get_hostname() << "' starting now...";
        SPDLOG_LOGGER_TRACE(
            downtimes_logger,
            "Flexible downtime (id={}) for service '{}' on host '{}' starting "
            "now...",
            dt.get_downtime_id(), svc->description(), svc->get_hostname());

        dt.start_flex_downtime();
        dt.handle();
      }
    }
  }
  return OK;
}

std::multimap<time_t, std::shared_ptr<downtime>> const&
downtime_manager::get_scheduled_downtimes() const {
  return _scheduled_downtimes;
}

void downtime_manager::clear_scheduled_downtimes() {
  _scheduled_downtimes.clear();
}

void downtime_manager::add_downtime(
    const std::shared_ptr<downtime>& dt) noexcept {
  _scheduled_downtimes.insert({dt->get_start_time(), dt});
}

int downtime_manager::check_for_expired_downtime() {
  time_t current_time(0L);

  engine_logger(dbg_functions, basic) << "check_for_expired_downtime()";
  SPDLOG_LOGGER_TRACE(functions_logger, "check_for_expired_downtime()");

  time(&current_time);

  /* check all downtime entries... */
  auto next_it = downtime_manager::instance()._scheduled_downtimes.begin();
  for (auto it = _scheduled_downtimes.begin(), end = _scheduled_downtimes.end();
       it != end; it = next_it) {
    downtime& dt(*it->second);
    ++next_it;

    /* this entry should be removed */
    if (!dt.is_in_effect() && dt.get_end_time() < current_time) {
      engine_logger(dbg_downtime, basic)
          << "Expiring "
          << (dt.get_type() == downtime::host_downtime ? "host" : "service")
          << " downtime (id=" << dt.get_downtime_id() << ")...";
      SPDLOG_LOGGER_TRACE(
          downtimes_logger, "Expiring {} downtime (id={})...",
          dt.get_type() == downtime::host_downtime ? "host" : "service",
          dt.get_downtime_id());

      /* delete the downtime entry */
      delete_downtime(dt.get_downtime_id());
    }
  }
  return OK;
}

/*
** Deletes all host and service downtimes on a host by hostname,
** optionally filtered by service description, start time and comment.
** All char* must be set or nullptr - "" will silently fail to match.
** Returns number deleted.
*/
int downtime_manager::
    delete_downtime_by_hostname_service_description_start_time_comment(
        std::string const& hostname,
        std::string const& service_description,
        std::pair<bool, time_t> const& start_time,
        std::string const& comment) {
  engine_logger(dbg_downtime, basic)
      << "Delete downtimes (host: '" << hostname << "', service description: '"
      << service_description << "', start time: " << start_time.second
      << ", comment: '" << comment << "')";
  SPDLOG_LOGGER_TRACE(
      downtimes_logger,
      "Delete downtimes (host: '{}', service description: '{}', start time: "
      "{}, comment: '{}')",
      hostname, service_description, start_time.second, comment);
  int deleted{0};

  /* Do not allow deletion of everything - must have at least 1 filter on. */
  if (hostname.empty() && service_description.empty() && !start_time.first &&
      comment.empty())
    return deleted;

  std::pair<std::multimap<time_t, std::shared_ptr<downtime>>::iterator,
            std::multimap<time_t, std::shared_ptr<downtime>>::iterator>
      range;

  if (start_time.first)
    range = _scheduled_downtimes.equal_range(start_time.second);
  else
    range = {_scheduled_downtimes.begin(), _scheduled_downtimes.end()};

  std::list<uint64_t> lst;
  for (auto it = range.first, end = range.second; it != end; ++it) {
    if (!comment.empty() && it->second->get_comment() != comment)
      continue;
    if (downtime::host_downtime == it->second->get_type()) {
      std::string name = engine::get_host_name(it->second->host_id());
      /* If service is specified, then do not delete the host downtime. */
      if (!service_description.empty())
        continue;
      if (!hostname.empty() && name != hostname)
        continue;
    } else if (downtime::service_downtime == it->second->get_type()) {
      service_downtime* sdt = static_cast<service_downtime*>(it->second.get());
      auto p = get_host_and_service_names(sdt->host_id(), sdt->service_id());
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
void downtime_manager::insert_downtime(std::shared_ptr<downtime> dt) {
  engine_logger(dbg_functions, basic) << "downtime_manager::insert_downtime()";
  SPDLOG_LOGGER_TRACE(functions_logger, "downtime_manager::insert_downtime()");
  time_t start{dt->get_start_time()};
  _scheduled_downtimes.insert({start, dt});
}

/**
 * Initialize downtime data
 *
 * @return OK or ERROR if an error occured.
 */
void downtime_manager::initialize_downtime_data() {
  engine_logger(dbg_functions, basic)
      << "downtime_manager::initialize_downtime_data()";
  SPDLOG_LOGGER_TRACE(functions_logger,
                      "downtime_manager::initialize_downtime_data()");
  /* clean up the old downtime data */
  xdddefault_validate_downtime_data();

  _next_id = 0;
}

/* removes invalid and old downtime entries from the downtime file */
int downtime_manager::xdddefault_validate_downtime_data() {
  bool save = true;

  /* remove stale downtimes */
  for (auto it = _scheduled_downtimes.begin(), end = _scheduled_downtimes.end();
       it != end;) {
    std::shared_ptr<com::centreon::engine::downtimes::downtime> temp_downtime(
        it->second);

    /* delete downtimes with invalid host names, invalid service descriptions
     * or that have expired. */
    if (temp_downtime->is_stale())
      it = _scheduled_downtimes.erase(it);
    else
      ++it;
  }

  /* remove triggered downtimes without valid parents */
  for (auto it = _scheduled_downtimes.begin(), end = _scheduled_downtimes.end();
       it != end;) {
    save = true;
    downtimes::downtime& temp_downtime(*it->second);

    if (!temp_downtime.get_triggered_by()) {
      ++it;
      continue;
    }

    if (!find_downtime(downtime::any_downtime,
                       temp_downtime.get_triggered_by()))
      save = false;

    /* delete the downtime */
    if (!save)
      it = _scheduled_downtimes.erase(it);
    else
      ++it;
  }

  return OK;
}

/**
 *  Return the next downtime id to use.
 *
 * @return an id as an unsigned long.
 */
uint64_t downtime_manager::get_next_downtime_id() {
  if (_next_id == 0) {
    for (auto const& dt : _scheduled_downtimes)
      if (dt.second->get_downtime_id() >= _next_id)
        _next_id = dt.second->get_downtime_id();
  }

  _next_id++;
  return _next_id;
}

/* saves a host downtime entry */
std::shared_ptr<host_downtime> downtime_manager::add_new_host_downtime(
    const uint64_t host_id,
    time_t entry_time,
    const char* author,
    const char* comment_data,
    time_t start_time,
    time_t end_time,
    bool fixed,
    uint64_t triggered_by,
    unsigned long duration) {
  auto found = host::hosts_by_id.find(host_id);
  if (found == host::hosts_by_id.end())
    throw engine_error() << "can not create a host downtime on host " << host_id
                         << " because it does not exist";

  host* hst = found->second.get();
  /* find the next valid downtime id */
  uint64_t new_downtime_id = get_next_downtime_id();

  /* add downtime to list in memory */
  auto retval{std::make_shared<host_downtime>(
      hst->host_id(), entry_time, author, comment_data, start_time, end_time,
      fixed, triggered_by, duration, new_downtime_id)};
  instance().add_downtime(retval);
  retval->schedule();

  /* send data to event broker */
  broker_downtime_data(NEBTYPE_DOWNTIME_ADD, NEBATTR_NONE,
                       downtime::host_downtime, hst->host_id(), 0, entry_time,
                       author, comment_data, start_time, end_time, fixed,
                       triggered_by, duration, new_downtime_id);
  return retval;
}

/* saves a service downtime entry */
std::shared_ptr<service_downtime> downtime_manager::add_new_service_downtime(
    const uint64_t host_id,
    const uint64_t service_id,
    time_t entry_time,
    const std::string& author,
    const std::string& comment_data,
    time_t start_time,
    time_t end_time,
    bool fixed,
    uint64_t triggered_by,
    unsigned long duration) {
  auto found = service::services_by_id.find({host_id, service_id});
  if (found == service::services_by_id.end())
    throw engine_error() << "can not create a service downtime on service ("
                         << host_id << ", " << service_id
                         << ") which does not exist";

  /* find the next valid downtime id */
  uint64_t new_downtime_id{get_next_downtime_id()};

  service* svc = found->second.get();

  /* add downtime to list in memory */
  auto retval{std::make_shared<service_downtime>(
      svc->host_id(), svc->service_id(), entry_time, author, comment_data,
      start_time, end_time, fixed, triggered_by, duration, new_downtime_id)};
  instance().add_downtime(retval);
  retval->schedule();

  /* send data to event broker */
  broker_downtime_data(NEBTYPE_DOWNTIME_ADD, NEBATTR_NONE,
                       downtime::service_downtime, svc->host_id(),
                       svc->service_id(), entry_time, author.c_str(),
                       comment_data.c_str(), start_time, end_time, fixed,
                       triggered_by, duration, new_downtime_id);
  return retval;
}

/* schedules a host or service downtime */
int downtime_manager::schedule_downtime(downtime::type type,
                                        const uint64_t host_id,
                                        const uint64_t service_id,
                                        time_t entry_time,
                                        char const* author,
                                        char const* comment_data,
                                        time_t start_time,
                                        time_t end_time,
                                        bool fixed,
                                        uint64_t triggered_by,
                                        unsigned long duration,
                                        uint64_t* new_downtime_id) {
  engine_logger(dbg_functions, basic) << "schedule_downtime()";
  SPDLOG_LOGGER_TRACE(functions_logger, "schedule_downtime()");

  /* don't add old or invalid downtimes */
  if (start_time >= end_time || end_time <= time(nullptr))
    return ERROR;

  if (start_time > 4102441200) {
    engine_logger(log_verification_error, basic)
        << "SCHEDULE DOWNTIME ALERT : start time is out of range and setted "
           "to "
           "1/1/2100 00:00";
    config_logger->warn(
        "SCHEDULE DOWNTIME ALERT : start time is out of range and setted to "
        "1/1/2100 00:00");
    start_time = 4102441200;
  }

  if (end_time > 4102441200) {
    engine_logger(log_verification_error, basic)
        << "SCHEDULE DOWNTIME ALERT : end time is out of range and setted to "
           "1/1/2100 00:00";
    config_logger->warn(
        "SCHEDULE DOWNTIME ALERT : end time is out of range and setted to "
        "1/1/2100 00:00");
    end_time = 4102441200;
  }

  if (duration > 31622400) {
    engine_logger(log_verification_error, basic)
        << "SCHEDULE DOWNTIME ALERT : is too long and setted to 366 days";
    config_logger->warn(
        "SCHEDULE DOWNTIME ALERT : is too long and setted to 366 days");
    duration = 31622400;
  }

  std::shared_ptr<downtime> dt;
  /* add a new downtime entry */
  if (type == downtime::host_downtime) {
    dt = add_new_host_downtime(host_id, entry_time, author, comment_data,
                               start_time, end_time, fixed, triggered_by,
                               duration);
  } else {
    dt = add_new_service_downtime(host_id, service_id, entry_time, author,
                                  comment_data, start_time, end_time, fixed,
                                  triggered_by, duration);

    // anomalydetection to put in downtime?
    auto found = service::services_by_id.find({host_id, service_id});

    if (found != service::services_by_id.end() && found->second) {
      const anomalydetection::pointer_set& anos =
          anomalydetection::get_anomaly(found->second->service_id());
      for (const anomalydetection* ano : anos) {
        uint64_t ano_downtime_id;
        downtime_manager::instance().schedule_downtime(
            downtime::service_downtime, ano->host_id(), ano->service_id(),
            entry_time, author, comment_data, start_time, end_time, fixed,
            dt->get_downtime_id(), duration, &ano_downtime_id);
      }
    }
  }
  /* return downtime id */
  if (new_downtime_id)
    *new_downtime_id = dt->get_downtime_id();

  /* register the scheduled downtime */
  register_downtime(type, dt->get_downtime_id());

  return OK;
}

/* registers scheduled downtime (schedules it, adds comments, etc.) */
int downtime_manager::register_downtime(downtime::type type,
                                        uint64_t downtime_id) {
  engine_logger(dbg_functions, basic)
      << "downtime_manager::register_downtime()";
  SPDLOG_LOGGER_TRACE(functions_logger,
                      "downtime_manager::register_downtime()");
  engine_logger(dbg_downtime, basic)
      << "register downtime(type: " << type << ", id: " << downtime_id << ")";
  SPDLOG_LOGGER_TRACE(downtimes_logger, "register downtime(type: {}, id: {})",
                      static_cast<uint32_t>(type), downtime_id);
  /* find the downtime entry in memory */
  std::shared_ptr<downtime> temp_downtime{find_downtime(type, downtime_id)};
  if (!temp_downtime)
    return ERROR;

  if (temp_downtime->subscribe() == ERROR)
    return ERROR;

  return OK;
}
