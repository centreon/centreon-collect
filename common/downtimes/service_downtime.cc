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

#include "common/downtimes/service_downtime.hh"

#include "com/centreon/engine/comment.hh"
#include "common/downtimes/downtime_manager.hh"

using namespace com::centreon::engine;

namespace com::centreon::common::downtimes {

service_downtime::service_downtime(
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
    uint64_t downtime_id,
    const std::shared_ptr<spdlog::logger>& logger)
    : downtime{downtime::service_downtime,
               host_id,
               entry_time,
               author,
               comment_data,
               start_time,
               end_time,
               fixed,
               triggered_by,
               duration,
               downtime_id,
               logger},
      _service_id{service_id} {}

service_downtime::~service_downtime() {
  comment::delete_comment(_get_comment_id());
  /* send data to event broker */
  downtime_manager::instance().callbacks().notify_broker(
      downtime_callbacks::DELETE, downtime_callbacks::ATTR_NONE, _host_id, _service_id, _author,
      _comment, _entry_time, _start_time, _end_time, _fixed, _triggered_by,
      _duration, _downtime_id);
}

/**
 *  This method tells if the associated host is no more here or if this downtime
 *  has expired.
 *
 * @return a boolean
 */
bool service_downtime::is_stale() const {
  bool retval = false;

  /* delete downtimes with invalid host names */
  if (!downtime_manager::instance().callbacks().host_exists(host_id()))
    retval = true;
  /* delete downtimes with invalid service descriptions */
  if (!downtime_manager::instance().callbacks().service_exists(host_id(),
                                                               service_id()))
    retval = true;
  /* delete downtimes that have expired */
  else if (get_end_time() < time(nullptr))
    retval = true;

  return retval;
}

/**
 *  Dump retention of downtime.
 *
 *  @param[out] os  The output stream.
 *  @param[in]  obj The downtime to dump.
 *
 *  @return The output stream.
 */
void service_downtime::retention(std::ostream& os) const {
  auto p = downtime_manager::instance().callbacks().get_host_and_service_names(
      host_id(), service_id());
  // If p.first starts with "_Module_BAM_" and p.second starts with 'ba_', we
  // skip this downtime.
  // The idea here is to avoid downtimes coming from BA, because broker already
  // sends them.
  if (p.first.compare(0, 12, "_Module_BAM_") == 0 &&
      p.second.compare(0, 3, "ba_") == 0)
    return;

  os << "servicedowntime {"
        "\nhost_name="
     << p.first << "\nservice_description=" << p.second
     << "\nauthor=" << get_author() << "\ncomment=" << get_comment()
     << "\nduration=" << get_duration()
     << "\nend_time=" << static_cast<uint32_t>(this->get_end_time())
     << "\nentry_time=" << static_cast<uint32_t>(get_entry_time())
     << "\nfixed=" << is_fixed()
     << "\nstart_time=" << static_cast<uint32_t>(get_start_time())
     << "\ntriggered_by=" << get_triggered_by()
     << "\ndowntime_id=" << get_downtime_id() << "\n}\n";
}

void service_downtime::print(std::ostream& os) const {
  auto p = downtime_manager::instance().callbacks().get_host_and_service_names(
      host_id(), service_id());
  // If p.first starts with "_Module_BAM_" and p.second starts with 'ba_', we
  // skip this downtime.
  // The idea here is to avoid downtimes coming from BA, because broker already
  // sends them.
  if (p.first.compare(0, 12, "_Module_BAM_") == 0 &&
      p.second.compare(0, 3, "ba_") == 0)
    return;

  os << "servicedowntime {\n"
        "\thost_name="
     << p.first
     << "\n"
        "\tservice_description="
     << p.second
     << "\n"
        "\tdowntime_id="
     << get_downtime_id()
     << "\n"
        "\tentry_time="
     << static_cast<unsigned long>(_entry_time)
     << "\n"
        "\tstart_time="
     << static_cast<unsigned long>(get_start_time())
     << "\n"
        "\tend_time="
     << static_cast<unsigned long>(get_end_time())
     << "\n"
        "\ttriggered_by="
     << get_triggered_by()
     << "\n"
        "\tfixed="
     << is_fixed()
     << "\n"
        "\tduration="
     << get_duration()
     << "\n"
        "\tauthor="
     << get_author()
     << "\n"
        "\tcomment="
     << get_comment()
     << "\n"
        "\t}\n\n";
}

bool service_downtime::unschedule() {
  SPDLOG_LOGGER_TRACE(_logger, "service_downtime::unschedule()");
  if (is_in_effect())
    downtime_manager::instance().callbacks().notify_broker(
        downtime_callbacks::STOP, downtime_callbacks::ATTR_STOP_CANCELLED,
        host_id(), service_id(), get_author(), get_comment(), _entry_time,
        get_start_time(), get_end_time(), is_fixed(), get_triggered_by(),
        get_duration(), get_downtime_id());
  return downtime_manager::instance().callbacks().cancel_downtime(
      host_id(), service_id(), is_fixed(), _incremented_pending_downtime,
      is_in_effect());
}

bool service_downtime::subscribe() {
  SPDLOG_LOGGER_TRACE(_logger, "service_downtime::subscribe()");

  auto found = service::services_by_id.find({host_id(), service_id()});

  /* find the host or service associated with this downtime */
  if (found == service::services_by_id.end() || !found->second)
    return false;

  /* create the comment */
  auto fmt_time = [](time_t t) -> std::string {
    std::tm tm_s;
    localtime_r(&t, &tm_s);
    char buf[64];
    std::strftime(buf, sizeof(buf), "%x %X", &tm_s);
    return buf;
  };
  std::string start_time_string{fmt_time(get_start_time())};
  std::string end_time_string{fmt_time(get_end_time())};
  uint32_t hours{get_duration() / 3600u};
  uint32_t minutes{(get_duration() - hours * 3600u) / 60u};
  uint32_t seconds{get_duration() - hours * 3600u - minutes * 60u};

  std::string msg;
  if (is_fixed())
    msg = fmt::format(
        "This service has been scheduled for fixed downtime from {} to {}. "
        "Notifications for the service will not be sent out during that time "
        "period.",
        start_time_string, end_time_string);
  else
    msg = fmt::format(
        "This service has been scheduled for flexible downtime starting between "
        "{} and {} and lasting for a period of {} hours and {} minutes. "
        "Notifications for the service will not be sent out during that time "
        "period.",
        start_time_string, end_time_string, hours, minutes);

  SPDLOG_LOGGER_TRACE(_logger, "Scheduled Downtime Details:");
  SPDLOG_LOGGER_TRACE(_logger, " Type: Service Downtime");
  SPDLOG_LOGGER_TRACE(_logger, " Host: {}", found->second->get_hostname());
  SPDLOG_LOGGER_TRACE(_logger, " Service: {}", found->second->description());

  SPDLOG_LOGGER_TRACE(
      _logger,
      " Fixed/Flex:  {} Start:       {} End:         {} Duration:    {}h "
      "{}m {}s Downtime ID: {} Trigger ID:  {}",
      is_fixed() ? "Fixed" : "Flexible", start_time_string, end_time_string,
      hours, minutes, seconds, get_downtime_id(), get_triggered_by());

  /* add a non-persistent comment to the host or service regarding the scheduled
   * outage */
  auto com{std::make_shared<comment>(
      comment::service, comment::downtime, found->second->host_id(),
      found->second->service_id(), time(nullptr), "(Centreon Engine Process)",
      msg, false, comment::internal, false, (time_t)0)};

  comment::comments.insert({com->get_comment_id(), com});
  _comment_id = com->get_comment_id();

  /*** SCHEDULE DOWNTIME - FLEXIBLE (NON-FIXED) DOWNTIME IS HANDLED AT A LATER
   * POINT ***/

  /* only non-triggered downtime is scheduled... */
  if (get_triggered_by() == 0)
    downtime_manager::instance().callbacks().schedule_downtime_check(
        get_downtime_id(), get_start_time());

  return true;
}

bool service_downtime::handle() {
  time_t event_time = 0L;

  SPDLOG_LOGGER_TRACE(_logger, "service_downtime::handle()");

  auto found = service::services_by_id.find({host_id(), service_id()});

  /* find the host or service associated with this downtime */
  if (found == service::services_by_id.end() || !found->second) {
    SPDLOG_LOGGER_ERROR(_logger, "{}:{} not found", host_id(), service_id());
    return false;
  }

  /* if downtime if flexible and host/svc is in an ok state, don't do anything
   * right now (wait for event handler to kick it off) */
  /* start_flex_downtime variable is set to true by event handler functions */
  if (!is_fixed()) {
    /* we're not supposed to force a start of flex downtime... */
    if (!_start_flex_downtime) {
      /* host is up or service is ok, so we don't really do anything right now
       */
      if (found->second->get_current_state() == service::state_ok) {
        /* increment pending flex downtime counter */
        found->second->inc_pending_flex_downtime();
        _incremented_pending_downtime = true;

        /*** SINCE THE FLEX DOWNTIME MAY NEVER START, WE HAVE TO PROVIDE A WAY
         * OF EXPIRING UNUSED DOWNTIME... ***/
        time_t temp;
        if (get_end_time() == INT64_MAX)
          temp = get_end_time();
        else
          temp = get_end_time() + 1;
        /*** Sometimes, get_end_time() == longlong::max(), if we add 1 to it,
         * it becomes < 0 ***/
        downtime_manager::instance().callbacks().schedule_expire_downtime(temp);
        return true;
      }
    }
  }

  /* have we come to the end of the scheduled downtime? */
  if (is_in_effect()) {
    /* send data to event broker */
    downtime_manager::instance().callbacks().notify_broker(
        downtime_callbacks::STOP, downtime_callbacks::ATTR_STOP_NORMAL, host_id(),
        service_id(), get_author(), get_comment(), _entry_time,
        get_start_time(), get_end_time(), is_fixed(), get_triggered_by(),
        get_duration(), get_downtime_id());

    /* decrement the downtime depth variable */
    found->second->dec_scheduled_downtime_depth();

    if (found->second->get_scheduled_downtime_depth() == 0) {
      SPDLOG_LOGGER_TRACE(
          _logger,
          "Service '{}' on host '{}' has exited from a period of "
          "scheduled downtime (id={}).",
          found->second->description(), found->second->get_hostname(),
          get_downtime_id());

      /* log a notice - this one is parsed by the history CGI */
      SPDLOG_LOGGER_INFO(
          _logger,
          "SERVICE DOWNTIME ALERT: {};{};STOPPED; Service has exited from a "
          "period of scheduled "
          "downtime",
          found->second->get_hostname(), found->second->description());

      /* send a notification */
      found->second->notify(notifier::reason_downtimeend, get_author(),
                            get_comment(), notifier::notification_option_none);
    }

    /* update the status data */
    /* We update with CHECK_RESULT level, so notifications numbers, downtimes,
     * and check infos will be updated. */
    found->second->update_status(service::STATUS_DOWNTIME_DEPTH);

    /* decrement pending flex downtime if necessary */
    if (!is_fixed() && _incremented_pending_downtime) {
      if (found->second->get_pending_flex_downtime() > 0)
        found->second->dec_pending_flex_downtime();
    }

    /* handle (stop) downtime that is triggered by this one */
    while (true) {
      /* list contents might change by recursive calls, so we restart from
       * scratch after each handle() call */
      bool found = false;
      for (auto
               it = downtime_manager::instance()
                        .get_scheduled_downtimes()
                        .begin(),
               end =
                   downtime_manager::instance().get_scheduled_downtimes().end();
           it != end; ++it) {
        if (it->second->get_triggered_by() == get_downtime_id()) {
          it->second->handle();
          found = true;
          break;
        }
      }
      if (!found)
        break;
    }

    /* delete downtime entry */
    downtime_manager::instance().delete_downtime(get_downtime_id());
  }
  /* else we are just starting the scheduled downtime */
  else {
    /* send data to event broker */
    downtime_manager::instance().callbacks().notify_broker(
        downtime_callbacks::START, downtime_callbacks::ATTR_NONE, host_id(), service_id(),
        get_author(), get_comment(), _entry_time, get_start_time(),
        get_end_time(), is_fixed(), get_triggered_by(), get_duration(),
        get_downtime_id());

    if (found->second->get_scheduled_downtime_depth() == 0) {
      SPDLOG_LOGGER_TRACE(
          _logger,
          "Service '{}' on host '{}' has entered a period of scheduled "
          "downtime (id={}).",
          found->second->description(), found->second->get_hostname(),
          get_downtime_id());

      /* log a notice - this one is parsed by the history CGI */
      SPDLOG_LOGGER_INFO(
          _logger,
          "SERVICE DOWNTIME ALERT: {};{};STARTED; Service has entered a period "
          "of scheduled downtime",
          found->second->get_hostname(), found->second->description());

      /* send a notification */
      found->second->notify(notifier::reason_downtimestart, get_author(),
                            get_comment(), notifier::notification_option_none);
    }

    /* increment the downtime depth variable */
    found->second->inc_scheduled_downtime_depth();

    /* set the in effect flag */
    _set_in_effect(true);

    /* update the status data */
    /* Because of the notification the status is sent with CHECK_RESULT level */
    found->second->update_status(service::STATUS_DOWNTIME_DEPTH);

    /* schedule an event */
    if (!is_fixed())
      event_time = (time_t)((unsigned long)time(nullptr) + get_duration());
    else {
      /* Sometimes, get_end_time() == longlong::max(), if we add 1 to it, it
       * becomes < 0 */
      if (get_end_time() == INT64_MAX)
        event_time = get_end_time();
      else
        event_time = get_end_time() + 1;
    }

    downtime_manager::instance().callbacks().schedule_downtime_check(
        get_downtime_id(), event_time);

    /* handle (start) downtime that is triggered by this one */
    std::multimap<time_t, std::shared_ptr<downtime>>::const_iterator it,
        end = downtime_manager::instance().get_scheduled_downtimes().end();

    for (it = downtime_manager::instance().get_scheduled_downtimes().begin();
         it != end; ++it) {
      if (it->second->get_triggered_by() == get_downtime_id())
        it->second->handle();
    }
  }
  return true;
}

uint64_t service_downtime::service_id() const {
  return _service_id;
}

/** @brief Fires NEBTYPE_DOWNTIME_LOAD for this service downtime. */
void service_downtime::notify_broker_load() {
  SPDLOG_LOGGER_TRACE(_logger, "service_downtime::notify_broker_load()");

  /* send data to event broker */
  downtime_manager::instance().callbacks().notify_broker(
      downtime_callbacks::LOAD, downtime_callbacks::ATTR_NONE, _host_id, _service_id, _author,
      _comment, _entry_time, _start_time, _end_time, _fixed, _triggered_by,
      _duration, _downtime_id);
}

}  // namespace com::centreon::common::downtimes
