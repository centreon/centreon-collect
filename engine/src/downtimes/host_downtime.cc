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

#include "com/centreon/engine/downtimes/host_downtime.hh"
#include "com/centreon/engine/broker.hh"
#include "com/centreon/engine/common.hh"
#include "com/centreon/engine/configuration/applier/state.hh"
#include "com/centreon/engine/downtimes/downtime_manager.hh"
#include "com/centreon/engine/events/loop.hh"
#include "com/centreon/engine/globals.hh"
#include "com/centreon/engine/logging/logger.hh"
#include "com/centreon/engine/statusdata.hh"
#include "com/centreon/engine/string.hh"

using namespace com::centreon::engine;
using namespace com::centreon::engine::configuration::applier;
using namespace com::centreon::engine::logging;
using namespace com::centreon::engine::downtimes;

host_downtime::host_downtime(const uint64_t host_id,
                             time_t entry_time,
                             std::string const& author,
                             std::string const& comment,
                             time_t start_time,
                             time_t end_time,
                             bool fixed,
                             uint64_t triggered_by,
                             int32_t duration,
                             uint64_t downtime_id)
    : downtime(downtime::host_downtime,
               host_id,
               entry_time,
               author,
               comment,
               start_time,
               end_time,
               fixed,
               triggered_by,
               duration,
               downtime_id) {}

host_downtime::~host_downtime() {
  comment::delete_comment(_get_comment_id());
  /* send data to event broker */
  broker_downtime_data(NEBTYPE_DOWNTIME_DELETE, NEBATTR_NONE,
                       downtime::host_downtime, host_id(), 0, _entry_time,
                       _author.c_str(), _comment.c_str(), get_start_time(),
                       get_end_time(), is_fixed(), get_triggered_by(),
                       get_duration(), get_downtime_id());
}

/* adds a host downtime entry to the list in memory */
/**
 *  This method tells if the associated host is no more here or if this downtime
 *  has expired.
 *
 * @return a boolean
 */
bool host_downtime::is_stale() const {
  bool retval = false;

  auto it = host::hosts_by_id.find(host_id());

  /* delete downtimes with invalid host names */
  if (it == host::hosts_by_id.end() || it->second == nullptr)
    retval = true;
  /* delete downtimes that have expired */
  else if (get_end_time() < time(nullptr))
    retval = true;

  return retval;
}

void host_downtime::retention(std::ostream& os) const {
  std::string name = engine::get_host_name(host_id());
  os << "hostdowntime {\n";
  os << "host_name=" << name << "\n";
  os << "author=" << get_author()
     << "\n"
        "comment="
     << get_comment()
     << "\n"
        "duration="
     << get_duration()
     << "\n"
        "end_time="
     << static_cast<unsigned long>(get_end_time())
     << "\n"
        "entry_time="
     << static_cast<unsigned long>(get_entry_time())
     << "\n"
        "fixed="
     << is_fixed()
     << "\n"
        "start_time="
     << static_cast<unsigned long>(get_start_time())
     << "\n"
        "triggered_by="
     << get_triggered_by()
     << "\n"
        "downtime_id="
     << get_downtime_id()
     << "\n"
        "}\n";
}

void host_downtime::print(std::ostream& os) const {
  std::string name = engine::get_host_name(host_id());
  os << "hostdowntime {\n";
  os << "\thost_name=" << name << "\n";
  os << "\tdowntime_id=" << get_downtime_id()
     << "\n"
        "\tentry_time="
     << static_cast<unsigned long>(get_entry_time())
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

int host_downtime::unschedule() {
  auto it = host::hosts_by_id.find(host_id());

  /* delete downtimes with invalid host names */
  if (it == host::hosts_by_id.end() || it->second == nullptr)
    return ERROR;

  /* decrement pending flex downtime if necessary ... */
  if (!is_fixed() && _incremented_pending_downtime)
    it->second->dec_pending_flex_downtime();

  /* decrement the downtime depth variable and update status data if necessary
   */
  if (is_in_effect()) {
    /* send data to event broker */
    broker_downtime_data(
        NEBTYPE_DOWNTIME_STOP, NEBATTR_DOWNTIME_STOP_CANCELLED, get_type(),
        host_id(), 0, _entry_time, get_author().c_str(), get_comment().c_str(),
        get_start_time(), get_end_time(), is_fixed(), get_triggered_by(),
        get_duration(), get_downtime_id());

    it->second->dec_scheduled_downtime_depth();
    it->second->update_status();

    /* log a notice - this is parsed by the history CGI */
    if (it->second->get_scheduled_downtime_depth() == 0) {
      engine_logger(log_info_message, basic)
          << "HOST DOWNTIME ALERT: " << it->second->name()
          << ";CANCELLED; Scheduled downtime for host has been "
             "cancelled.";
      events_logger->info(
          "HOST DOWNTIME ALERT: {};CANCELLED; Scheduled downtime for host has "
          "been "
          "cancelled.",
          it->second->name());

      /* send a notification */
      it->second->notify(notifier::reason_downtimecancelled, "", "",
                         notifier::notification_option_none);
    }
  }
  return OK;
}

int host_downtime::subscribe() {
  engine_logger(dbg_functions, basic) << "host_downtime::subscribe()";
  functions_logger->trace("host_downtime::subscribe()");

  auto it = host::hosts_by_id.find(host_id());

  /* find the host or service associated with this downtime */
  if (it == host::hosts_by_id.end() || it->second == nullptr)
    return ERROR;

  host* hst = it->second.get();

  /* create the comment */
  time_t start_time{get_start_time()};
  time_t end_time{get_end_time()};
  char start_time_string[MAX_DATETIME_LENGTH] = "";
  char end_time_string[MAX_DATETIME_LENGTH] = "";
  get_datetime_string(&start_time, start_time_string, MAX_DATETIME_LENGTH,
                      SHORT_DATE_TIME);
  get_datetime_string(&end_time, end_time_string, MAX_DATETIME_LENGTH,
                      SHORT_DATE_TIME);
  int hours{get_duration() / 3600};
  int minutes{(get_duration() - hours * 3600) / 60};
  int seconds{get_duration() - hours * 3600 - minutes * 60};

  char const* type_string{"host"};
  std::string msg;
  if (is_fixed())
    msg = fmt::format(
        "This {}"
        " has been scheduled for fixed downtime from {}"
        " to {} Notifications for the {}"
        " will not be sent out during that time period.",
        type_string, start_time_string, end_time_string, type_string);
  else
    msg = fmt::format(
        "This {0}"
        " has been scheduled for flexible downtime starting between {1}"
        " and {2}"
        " and lasting for a period of {3} hours and {4}"
        " minutes. Notifications for the {0}"
        " will not be sent out during that time period.",
        type_string, start_time_string, end_time_string, hours, minutes);

  engine_logger(dbg_downtime, basic) << "Scheduled Downtime Details:";
  downtimes_logger->trace("Scheduled Downtime Details:");
  engine_logger(dbg_downtime, basic) << " Type:        Host Downtime\n"
                                        " Host:        "
                                     << hst->name();
  downtimes_logger->trace(" Type: Host Downtime ; Host: {}", hst->name());
  engine_logger(dbg_downtime, basic)
      << " Fixed/Flex:  " << (is_fixed() ? "Fixed\n" : "Flexible\n")
      << " Start:       " << start_time_string
      << "\n"
         " End:         "
      << end_time_string
      << "\n"
         " Duration:    "
      << hours << "h " << minutes << "m " << seconds
      << "s\n"
         " Downtime ID: "
      << get_downtime_id()
      << "\n"
         " Trigger ID:  "
      << get_triggered_by();
  downtimes_logger->trace(
      " Fixed/Flex:  {} Start:       {} End:         {} Duration:    {}h "
      "{}m {}s Downtime ID: {} Trigger ID:  ",
      is_fixed() ? "Fixed" : "Flexible", start_time_string, end_time_string,
      hours, minutes, get_downtime_id(), get_triggered_by());

  /* add a non-persistent comment to the host or service regarding the scheduled
   * outage */
  auto com = std::make_shared<comment>(comment::host, comment::downtime,
                                       hst->host_id(), 0, time(nullptr),
                                       "(Centreon Engine Process)", msg, false,
                                       comment::internal, false, (time_t)0);

  comment::comments.insert({com->get_comment_id(), com});
  _comment_id = com->get_comment_id();

  /*** SCHEDULE DOWNTIME - FLEXIBLE (NON-FIXED) DOWNTIME IS HANDLED AT A LATER
   * POINT ***/

  /* only non-triggered downtime is scheduled... */
  if (get_triggered_by() == 0) {
    uint64_t* new_downtime_id{new uint64_t{get_downtime_id()}};
    events::loop::instance().schedule(
        std::make_unique<timed_event>(
            timed_event::EVENT_SCHEDULED_DOWNTIME, get_start_time(), false, 0,
            nullptr, false, (void*)new_downtime_id, nullptr, 0),
        true);
  }

#ifdef PROBABLY_NOT_NEEDED
  /*** FLEXIBLE DOWNTIME SANITY CHECK - ADDED 02/17/2008 ****/

  /* if host/service is in a non-OK/UP state right now, see if we should start
   * flexible time immediately */
  /* this is new logic added in 3.0rc3 */
  if (!this->fixed) {
    check_pending_flex_host_downtime(hst);
  }
#endif
  return OK;
}

int host_downtime::handle() {
  time_t event_time{0L};
  int attr{0};

  engine_logger(dbg_functions, basic) << "handle_downtime()";
  functions_logger->trace("handle_downtime()");

  auto it_hst = host::hosts_by_id.find(host_id());

  /* find the host or service associated with this downtime */
  if (it_hst == host::hosts_by_id.end() || it_hst->second == nullptr)
    return ERROR;

  /* if downtime is flexible and host/svc is in an ok state, don't do anything
   * right now (wait for event handler to kick it off) */
  /* start_flex_downtime variable is set to true by event handler functions */
  if (!is_fixed()) {
    /* we're not supposed to force a start of flex downtime... */
    if (!_start_flex_downtime) {
      /* host is up, so we don't really do anything right now */
      if (it_hst->second->get_current_state() == host::state_up) {
        /* increment pending flex downtime counter */
        it_hst->second->inc_pending_flex_downtime();
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
        events::loop::instance().schedule(
            std::make_unique<timed_event>(timed_event::EVENT_EXPIRE_DOWNTIME,
                                          temp, false, 0, nullptr, false,
                                          nullptr, nullptr, 0),
            true);
        return OK;
      }
    }
  }

  /* have we come to the end of the scheduled downtime? */
  if (is_in_effect()) {
    /* send data to event broker */
    attr = NEBATTR_DOWNTIME_STOP_NORMAL;
    broker_downtime_data(NEBTYPE_DOWNTIME_STOP, attr, get_type(), host_id(), 0,
                         _entry_time, get_author().c_str(),
                         get_comment().c_str(), get_start_time(),
                         get_end_time(), is_fixed(), get_triggered_by(),
                         get_duration(), get_downtime_id());

    /* decrement the downtime depth variable */
    it_hst->second->dec_scheduled_downtime_depth();

    if (it_hst->second->get_scheduled_downtime_depth() == 0) {
      engine_logger(dbg_downtime, basic)
          << "Host '" << it_hst->second->name()
          << "' has exited from a period of scheduled downtime (id="
          << get_downtime_id() << ").";
      downtimes_logger->trace(
          "Host '{}' has exited from a period of scheduled downtime (id={}).",
          it_hst->second->name(), get_downtime_id());

      /* log a notice - this one is parsed by the history CGI */
      engine_logger(log_info_message, basic)
          << "HOST DOWNTIME ALERT: " << it_hst->second->name()
          << ";STOPPED; Host has exited from a period of scheduled "
             "downtime";
      events_logger->info(
          "HOST DOWNTIME ALERT: {};STOPPED; Host has exited from a period of "
          "scheduled "
          "downtime",
          it_hst->second->name());

      /* send a notification */
      it_hst->second->notify(notifier::reason_downtimeend, get_author(),
                             get_comment(), notifier::notification_option_none);
    }

    /* update the status data */
    it_hst->second->update_status();

    /* decrement pending flex downtime if necessary */
    if (!is_fixed() && _incremented_pending_downtime) {
      if (it_hst->second->get_pending_flex_downtime() > 0)
        it_hst->second->dec_pending_flex_downtime();
    }

    /* handle (stop) downtime that is triggered by this one */
    while (true) {
      std::multimap<time_t, std::shared_ptr<downtime>>::const_iterator it;
      std::multimap<time_t, std::shared_ptr<downtime>>::const_iterator end{
          downtime_manager::instance().get_scheduled_downtimes().end()};

      /*
       * list contents might change by recursive calls, so we use this
       * inefficient method to prevent segfaults
       */
      for (it = downtime_manager::instance().get_scheduled_downtimes().begin();
           it != end; ++it) {
        if (it->second->get_triggered_by() == get_downtime_id()) {
          it->second->handle();
          break;
        }
      }

      for (it = downtime_manager::instance().get_scheduled_downtimes().begin();
           it != end; ++it) {
        if (it->second->get_triggered_by() == get_downtime_id()) {
          it->second->handle();
          break;
        }
      }

      if (it == end)
        break;
    }

    /* delete downtime entry */
    downtime_manager::instance().delete_downtime(get_downtime_id());
  }
  /* else we are just starting the scheduled downtime */
  else {
    /* send data to event broker */
    broker_downtime_data(NEBTYPE_DOWNTIME_START, NEBATTR_NONE, get_type(),
                         host_id(), 0, _entry_time, get_author().c_str(),
                         get_comment().c_str(), get_start_time(),
                         get_end_time(), is_fixed(), get_triggered_by(),
                         get_duration(), get_downtime_id());

    if (it_hst->second->get_scheduled_downtime_depth() == 0) {
      engine_logger(dbg_downtime, basic)
          << "Host '" << it_hst->second->name()
          << "' has entered a period of scheduled downtime (id="
          << get_downtime_id() << ").";
      downtimes_logger->trace(
          "Host '{}' has entered a period of scheduled downtime (id={}).",
          it_hst->second->name(), get_downtime_id());

      /* log a notice - this one is parsed by the history CGI */
      engine_logger(log_info_message, basic)
          << "HOST DOWNTIME ALERT: " << it_hst->second->name()
          << ";STARTED; Host has entered a period of scheduled downtime";
      events_logger->info(
          "HOST DOWNTIME ALERT: {};STARTED; Host has entered a period of "
          "scheduled downtime",
          it_hst->second->name());

      /* send a notification */
      it_hst->second->notify(notifier::reason_downtimestart, get_author(),
                             get_comment(), notifier::notification_option_none);
    }

    /* increment the downtime depth variable */
    it_hst->second->inc_scheduled_downtime_depth();

    /* set the in effect flag */
    _set_in_effect(true);

    /* update the status data */
    /* Because of the notification the status is sent with CHECK_RESULT level */
    it_hst->second->update_status(host::STATUS_DOWNTIME_DEPTH);

    /* schedule an event */
    if (!is_fixed())
      event_time = (time_t)((uint64_t)time(nullptr) + get_duration());
    else {
      /* Sometimes, get_end_time() == longlong::max(), if we add 1 to it, it
       * becomes < 0 */
      if (get_end_time() == INT64_MAX)
        event_time = get_end_time();
      else
        event_time = get_end_time() + 1;
    }

    uint64_t* new_downtime_id{new uint64_t{get_downtime_id()}};
    events::loop::instance().schedule(
        std::make_unique<timed_event>(timed_event::EVENT_SCHEDULED_DOWNTIME,
                                      event_time, false, 0, nullptr, false,
                                      (void*)new_downtime_id, nullptr, 0),
        true);

    /* handle (start) downtime that is triggered by this one */
    std::multimap<time_t, std::shared_ptr<downtime>>::const_iterator it,
        end{downtime_manager::instance().get_scheduled_downtimes().end()};

    for (it = downtime_manager::instance().get_scheduled_downtimes().begin();
         it != end; ++it) {
      if (it->second->get_triggered_by() == get_downtime_id())
        it->second->handle();
    }
  }
  return OK;
}

void host_downtime::schedule() {
  /* send data to event broker */
  broker_downtime_data(
      NEBTYPE_DOWNTIME_LOAD, NEBATTR_NONE, downtime::host_downtime, host_id(),
      0, _entry_time, _author.c_str(), _comment.c_str(), _start_time, _end_time,
      _fixed, _triggered_by, _duration, _downtime_id);
}
