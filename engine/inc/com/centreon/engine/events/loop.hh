/*
** Copyright 1999-2009 Ethan Galstad
** Copyright 2009-2010 Nagios Core Development Team and Community Contributors
** Copyright 2011-2013 Merethis
**
** This file is part of Centreon Engine.
**
** Centreon Engine is free software: you can redistribute it and/or
** modify it under the terms of the GNU General Public License version 2
** as published by the Free Software Foundation.
**
** Centreon Engine is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
** General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with Centreon Engine. If not, see
** <http://www.gnu.org/licenses/>.
*/

#ifndef CCE_EVENTS_LOOP_HH
#define CCE_EVENTS_LOOP_HH

#include "com/centreon/engine/events/timed_event.hh"

using timed_event_list =
    std::deque<std::unique_ptr<com::centreon::engine::timed_event>>;

namespace com::centreon::engine {

namespace events {
/**
 *  @class loop loop.hh
 *  @brief Create Centreon Engine event loop on a new thread.
 *
 *  Events loop is a singleton to create a new thread
 *  and dispatch the Centreon Engine events.
 */
class loop {
  time_t _last_status_update;
  time_t _last_time;
  unsigned int _need_reload;

  bool _reload_running;
  timed_event _sleep_event;

  timed_event_list _event_list_high;
  timed_event_list _event_list_low;

  loop();
  loop(const loop&) = delete;
  ~loop() noexcept = default;
  loop& operator=(const loop&) = delete;
  void _dispatching();

 public:
  enum priority { low = 0, high = 1, priority_num };

  static loop& instance();
  void clear();
  void run();
  void add_event(std::unique_ptr<timed_event>&& event, priority priority);
  void compensate_for_system_time_change(unsigned long last_time,
                                         unsigned long current_time);
  void remove_downtime(uint64_t downtime_id);
  void remove_event(timed_event_list::iterator& it, priority priority);
  void remove_event(timed_event* evt, loop::priority priority);
  void remove_events(priority, uint32_t event_type, void* data) noexcept;
  timed_event_list::iterator find_event(priority priority,
                                        uint32_t event_type,
                                        void* data);
  const timed_event_list::const_iterator list_end(
      priority priority) const noexcept {
    if (priority == low)
      return _event_list_low.end();
    else
      return _event_list_high.end();
  }

  void reschedule_event(std::unique_ptr<timed_event>&& event,
                        priority priority);
  void resort_event_list(priority priority);
  void schedule(std::unique_ptr<timed_event>&& evt, bool high_priority);
};
}  // namespace events

}  // namespace com::centreon::engine

#endif  // !CCE_EVENTS_LOOP_HH
