/**
 * Copyright 2011 - 2013, 2026 Centreon (https://www.centreon.com/)
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

/**
 * @brief Construct a downtime entry for a host or service.
 *
 * @param host_id       ID of the host targeted by this downtime.
 * @param service_id    ID of the service targeted by this downtime, or 0 for a
 *                      host downtime.
 * @param entry_time    Time at which this downtime was created.
 * @param author        Author who scheduled the downtime.
 * @param comment       Comment associated with the downtime.
 * @param start_time    Scheduled start time.
 * @param end_time      Scheduled end time.
 * @param fixed         True if the downtime is fixed, false if flexible.
 * @param triggered_by  ID of the parent downtime that triggered this one, or 0.
 * @param duration      Duration in seconds (used for flexible downtimes).
 * @param downtime_id   Unique identifier for this downtime.
 * @param logger        Logger instance.
 * @throw msg_fmt if triggered_by is non-zero and the parent downtime does not
 *                exist.
 */
downtime::downtime(uint64_t host_id,
                   uint64_t service_id,
                   time_t entry_time,
                   const std::string& author,
                   const std::string& comment,
                   time_t start_time,
                   time_t end_time,
                   bool fixed,
                   uint64_t triggered_by,
                   uint32_t duration,
                   uint64_t downtime_id,
                   const std::shared_ptr<spdlog::logger>& logger)
    : _host_id{host_id},
      _service_id{service_id},
      _entry_time{entry_time},
      _author{author},
      _comment{comment},
      _start_time{start_time},
      _end_time{end_time},
      _fixed{fixed},
      _triggered_by{triggered_by},
      _duration{duration},
      _downtime_id{downtime_id},
      _in_effect{false},
      _comment_id{0},
      _start_flex_downtime{0},
      _incremented_pending_downtime{false},
      _logger{logger} {
  /* don't add triggered downtimes that don't have a valid parent */
  if (triggered_by > 0 && !downtime_manager::instance().find_downtime(
                              downtime::any_downtime, triggered_by))
    throw msg_fmt("can not add triggered host downtime without a valid parent");
}

/**
 * @brief Destructor. Deletes the associated comment and notifies the broker of
 *        the downtime deletion.
 */
downtime::~downtime() {
  /* When the downtime_manager itself is being torn down (unload()), its
   * _instance is already null while the remaining scheduled downtimes are
   * destroyed. Calling instance() then would assert. In that case there is
   * nothing/no one left to notify, so just skip. */
  if (!downtime_manager::is_loaded())
    return;
  downtime_manager::instance().callbacks().delete_downtime_comment(
      _get_comment_id());
  /* send data to event broker */
  downtime_manager::instance().callbacks().notify_broker(
      downtime_callbacks::DELETE, downtime_callbacks::ATTR_NONE, _host_id,
      _service_id, _author, _comment, _entry_time, _start_time, _end_time,
      _fixed, _triggered_by, _duration, _downtime_id);
}

downtime::type downtime::get_type() const {
  return _service_id == 0 ? host_downtime : service_downtime;
}

uint64_t downtime::service_id() const {
  return _service_id;
}

uint64_t downtime::host_id() const {
  return _host_id;
}

const std::string& downtime::get_comment() const {
  return _comment;
}

const std::string& downtime::get_author() const {
  return _author;
}

uint64_t downtime::get_downtime_id() const {
  return _downtime_id;
}

uint64_t downtime::get_triggered_by() const {
  return _triggered_by;
}

bool downtime::is_fixed() const {
  return _fixed;
}

time_t downtime::get_entry_time() const {
  return _entry_time;
}

time_t downtime::get_start_time() const {
  return _start_time;
}

time_t downtime::get_end_time() const {
  return _end_time;
}

uint32_t downtime::get_duration() const {
  return _duration;
}

bool downtime::is_in_effect() const {
  return _in_effect;
}

void downtime::_set_in_effect(bool in_effect) {
  _in_effect = in_effect;
}

uint64_t downtime::_get_comment_id() const {
  return _comment_id;
}

void downtime::start_flex_downtime() {
  _start_flex_downtime = true;
}

/**
 * @brief Check whether this downtime entry is stale and should be removed.
 *
 * A downtime is stale if the host or service it targets no longer exists, or if
 * its end time is in the past.
 *
 * @return true if the downtime is stale, false otherwise.
 */
bool downtime::is_stale() const {
  if (!downtime_manager::instance().callbacks().host_exists(_host_id))
    return true;
  if (_service_id != 0 &&
      !downtime_manager::instance().callbacks().service_exists(_host_id,
                                                               _service_id))
    return true;
  if (get_end_time() < time(nullptr))
    return true;
  return false;
}

/**
 * @brief Notify the broker that this downtime has been loaded from retention.
 */
void downtime::notify_broker_load() {
  downtime_manager::instance().callbacks().notify_broker(
      downtime_callbacks::LOAD, downtime_callbacks::ATTR_NONE, _host_id,
      _service_id, _author, _comment, _entry_time, _start_time, _end_time,
      _fixed, _triggered_by, _duration, _downtime_id);
}

/**
 * @brief Unschedule this downtime, cancelling it if it is currently in effect.
 *
 * @return true on success, false if the cancellation callback failed.
 */
bool downtime::unschedule() {
  if (is_in_effect())
    downtime_manager::instance().callbacks().notify_broker(
        downtime_callbacks::STOP, downtime_callbacks::ATTR_STOP_CANCELLED,
        _host_id, _service_id, _author, _comment, _entry_time, _start_time,
        _end_time, _fixed, _triggered_by, _duration, _downtime_id);
  return downtime_manager::instance().callbacks().cancel_downtime(
      _host_id, _service_id, _fixed, _incremented_pending_downtime,
      is_in_effect());
}

/**
 * @brief Register the downtime in the scheduler and post the associated
 *        comment.
 *
 * Creates an internal comment describing the downtime window and schedules the
 * start-time check event. For triggered downtimes (triggered_by != 0), no
 * check event is scheduled here; the parent downtime handles the trigger.
 *
 * @return true on success, false if the target object no longer exists.
 */
bool downtime::subscribe() {
  _logger->trace("downtime::subscribe() id={}", _downtime_id);

  if (!downtime_manager::instance().callbacks().resource_exists(_host_id,
                                                                _service_id))
    return false;

  auto fmt_time = [](time_t t) -> std::string {
    std::tm tm_s;
    localtime_r(&t, &tm_s);
    char buf[64];
    std::strftime(buf, sizeof(buf), "%x %X", &tm_s);
    return buf;
  };
  std::string start_time_string{fmt_time(get_start_time())};
  std::string end_time_string{fmt_time(get_end_time())};
  uint32_t hours{_duration / 3600u};
  uint32_t minutes{(_duration - hours * 3600u) / 60u};
  uint32_t seconds{_duration - hours * 3600u - minutes * 60u};

  const std::string_view type_str{_service_id == 0 ? "host" : "service"};
  std::string msg;
  if (_fixed)
    msg = fmt::format(
        "This {} has been scheduled for fixed downtime from {} to {}. "
        "Notifications for the {} will not be sent out during that time "
        "period.",
        type_str, start_time_string, end_time_string, type_str);
  else
    msg = fmt::format(
        "This {} has been scheduled for flexible downtime starting between "
        "{} and {} and lasting for a period of {} hours and {} minutes. "
        "Notifications for the {} will not be sent out during that time "
        "period.",
        type_str, start_time_string, end_time_string, hours, minutes, type_str);

  _logger->trace("Scheduled Downtime Details:");
  if (_service_id == 0) {
    std::string name =
        downtime_manager::instance().callbacks().get_host_name(_host_id);
    _logger->trace(" Type: Host Downtime ; Host: {}", name);
  } else {
    auto [host_name, svc_desc] =
        downtime_manager::instance().callbacks().get_host_and_service_names(
            _host_id, _service_id);
    _logger->trace(" Type: Service Downtime ; Host: {} ; Service: {}",
                   host_name, svc_desc);
  }
  _logger->trace(
      " Fixed/Flex: {} Start: {} End: {} Duration: {}h {}m {}s "
      "Downtime ID: {} Trigger ID: {}",
      _fixed ? "Fixed" : "Flexible", start_time_string, end_time_string, hours,
      minutes, seconds, _downtime_id, _triggered_by);

  /* add a non-persistent comment */
  _comment_id =
      downtime_manager::instance().callbacks().create_downtime_comment(
          _host_id, _service_id, "(Centreon Engine Process)", msg);

  if (_triggered_by == 0)
    downtime_manager::instance().callbacks().schedule_downtime_check(
        _downtime_id, _start_time);

  return true;
}

/**
 * @brief Handle a downtime event: start or stop the downtime depending on its
 *        current state.
 *
 * When the downtime is not yet in effect, this method starts it: notifies the
 * broker, applies the downtime effect on the object, marks it in effect, and
 * schedules the end-time event. It also triggers any child downtimes.
 *
 * When the downtime is already in effect (end-time event fired), this method
 * stops it: notifies the broker, removes the downtime effect, handles triggered
 * children, and deletes the entry.
 *
 * For flexible downtimes that have not yet started (object currently OK), the
 * pending flex counter is incremented and an expiry event is scheduled instead.
 *
 * @return true on success, false if the target object no longer exists.
 */
bool downtime::handle() {
  _logger->trace("downtime::handle() id={}", _downtime_id);

  if (!downtime_manager::instance().callbacks().resource_exists(_host_id,
                                                                _service_id)) {
    _logger->error("downtime::handle(): object {}:{} not found", _host_id,
                   _service_id);
    return false;
  }

  if (!_fixed && !_start_flex_downtime) {
    if (downtime_manager::instance().callbacks().is_resource_ok(_host_id,
                                                                _service_id)) {
      _incremented_pending_downtime =
          downtime_manager::instance().callbacks().inc_pending_flex_downtime(
              _host_id, _service_id);
      time_t temp;
      if (_end_time == INT64_MAX)
        temp = _end_time;
      else
        temp = _end_time + 1;
      downtime_manager::instance().callbacks().schedule_expire_downtime(temp);
      return true;
    }
  }

  if (is_in_effect()) {
    downtime_manager::instance().callbacks().notify_broker(
        downtime_callbacks::STOP, downtime_callbacks::ATTR_STOP_NORMAL,
        _host_id, _service_id, _author, _comment, _entry_time, _start_time,
        _end_time, _fixed, _triggered_by, _duration, _downtime_id);

    downtime_manager::instance().callbacks().end_downtime_effect(
        _host_id, _service_id, _fixed, _incremented_pending_downtime, _author,
        _comment);

    /* handle triggered downtimes */
    while (true) {
      bool found{false};
      for (auto& [_, dt] :
           downtime_manager::instance().get_scheduled_downtimes()) {
        if (dt->get_triggered_by() == _downtime_id) {
          dt->handle();
          found = true;
          break;
        }
      }
      if (!found)
        break;
    }

    downtime_manager::instance().delete_downtime(_downtime_id);
  } else {
    downtime_manager::instance().callbacks().notify_broker(
        downtime_callbacks::START, downtime_callbacks::ATTR_NONE, _host_id,
        _service_id, _author, _comment, _entry_time, _start_time, _end_time,
        _fixed, _triggered_by, _duration, _downtime_id);

    downtime_manager::instance().callbacks().start_downtime_effect(
        _host_id, _service_id, _author, _comment);

    _set_in_effect(true);

    time_t event_time;
    if (!_fixed)
      event_time = (time_t)((uint64_t)time(nullptr) + _duration);
    else
      event_time = (_end_time == INT64_MAX) ? _end_time : _end_time + 1;

    downtime_manager::instance().callbacks().schedule_downtime_check(
        _downtime_id, event_time);

    for (auto& [_, dt] :
         downtime_manager::instance().get_scheduled_downtimes()) {
      if (dt->get_triggered_by() == _downtime_id)
        dt->handle();
    }
  }
  return true;
}

/**
 * @brief Write this downtime to @p os in the retention file format.
 *
 * Service downtimes whose host name starts with "_Module_BAM_" and whose
 * service description starts with "ba_" are silently skipped because broker
 * handles BAM downtimes itself.
 *
 * @param os Output stream to write to.
 */
void downtime::retention(std::ostream& os) const {
  if (_service_id == 0) {
    std::string name =
        downtime_manager::instance().callbacks().get_host_name(_host_id);
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
  } else {
    auto p =
        downtime_manager::instance().callbacks().get_host_and_service_names(
            _host_id, _service_id);
    // If p.first starts with "_Module_BAM_" and p.second starts with 'ba_', we
    // skip this downtime.
    // The idea here is to avoid downtimes coming from BA, because broker
    // already sends them.
    if (p.first.compare(0, 12, "_Module_BAM_") == 0 &&
        p.second.compare(0, 3, "ba_") == 0)
      return;

    os << "servicedowntime {"
          "\nhost_name="
       << p.first << "\nservice_description=" << p.second
       << "\nauthor=" << get_author() << "\ncomment=" << get_comment()
       << "\nduration=" << get_duration()
       << "\nend_time=" << static_cast<uint32_t>(get_end_time())
       << "\nentry_time=" << static_cast<uint32_t>(get_entry_time())
       << "\nfixed=" << is_fixed()
       << "\nstart_time=" << static_cast<uint32_t>(get_start_time())
       << "\ntriggered_by=" << get_triggered_by()
       << "\ndowntime_id=" << get_downtime_id() << "\n}\n";
  }
}

/**
 * @brief Write this downtime to @p os in the human-readable debug format.
 *
 * Same BAM-skip rule as retention(). Used for logging and diagnostics.
 *
 * @param os Output stream to write to.
 */
void downtime::print(std::ostream& os) const {
  if (_service_id == 0) {
    std::string name =
        downtime_manager::instance().callbacks().get_host_name(_host_id);
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
  } else {
    auto p =
        downtime_manager::instance().callbacks().get_host_and_service_names(
            _host_id, _service_id);
    // If p.first starts with "_Module_BAM_" and p.second starts with 'ba_', we
    // skip this downtime.
    // The idea here is to avoid downtimes coming from BA, because broker
    // already sends them.
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
}

}  // namespace com::centreon::common::downtimes

/**
 * @brief Handle a scheduled downtime identified by its ID.
 *
 * Called from the timed event queue when a start or end event fires.
 *
 * @param downtime_id  ID of the downtime to handle.
 * @return true if the downtime was found and handled, false otherwise.
 */
bool handle_scheduled_downtime_by_id(uint64_t downtime_id) {
  using namespace com::centreon::common::downtimes;
  std::shared_ptr<downtime> temp_downtime{
      downtime_manager::instance().find_downtime(downtime::any_downtime,
                                                 downtime_id)};
  /* find the downtime entry */
  if (!temp_downtime)
    return false;

  /* handle the downtime */
  return temp_downtime->handle();
}

std::ostream& operator<<(std::ostream& os,
                         com::centreon::common::downtimes::downtime const& dt) {
  dt.print(os);
  return os;
}
