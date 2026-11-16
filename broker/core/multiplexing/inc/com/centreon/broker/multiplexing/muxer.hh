/**
 * Copyright 2009-2017,2023 Centreon
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * For more information : contact@centreon.com
 */

#ifndef CCB_MULTIPLEXING_MUXER_HH
#define CCB_MULTIPLEXING_MUXER_HH

#include <absl/container/flat_hash_map.h>

#include "com/centreon/broker/multiplexing/engine.hh"
#include "com/centreon/broker/multiplexing/muxer_filter.hh"
#include "com/centreon/broker/persistent_file.hh"

namespace com::centreon::broker::multiplexing {
/**
 *  @class muxer muxer.hh "com/centreon/broker/multiplexing/muxer.hh"
 *  @brief Receive and send events from/to the multiplexing engine.
 *
 *  This class is a cornerstone in event multiplexing. Each endpoint
 *  willing to communicate with the multiplexing engine will create a
 *  new muxer object. This objects broadcast events sent to it to all
 *  other muxer objects.
 *
 *  It works essentially with 3 methods:
 *  * publish(): only called from multiplexing::engine. The event is stored to
 * the muxer queue if it is possible, otherwise, it is written to its retention
 * file.
 *  * write(): it is called from the other side (failover or feeder) to send an
 * event to the muxer. This time, the muxer does not push it to its queue, but
 * calls the engine publisher who will publish this event to all its muxers and
 * also this one.
 *  * read(): it is used to get the next available event for the
 * failover/feeder.
 *
 *  @see engine
 */
class muxer : public io::stream, public std::enable_shared_from_this<muxer> {
 public:
  class data_handler {
   public:
    virtual ~data_handler() = default;
    virtual uint32_t on_events(
        const std::vector<std::shared_ptr<io::data>>& events) = 0;
  };

 private:
  /* The three priority levels, in read order:
   *   P (priority_level)   — pb_downtime, pb_acknowledgement
   *   C (current_level)    — fresh host/service status events
   *   H (historical_level) — bulk data, logs, old status events */
  enum level : uint8_t {
    priority_level = 0,
    current_level,
    historical_level,
    nb_levels
  };

  /* The events of one level, in publication order. An event stays in `events`
   * until it is acknowledged: the first `read` ones have been read and wait
   * for their acknowledgement, the others are not read yet. Reading an event
   * only increments `read`, nothing is moved. */
  struct level_queue {
    struct entry {
      std::shared_ptr<io::data> event;
      int64_t timestamp;  // publication time, used to demote stale C events
    };
    std::deque<entry> events;
    size_t read = 0;

    size_t unread() const noexcept { return events.size() - read; }
  };

  /* `count` events of the level `lvl` read in a row. */
  struct read_run {
    level lvl;
    size_t count;
  };

  static uint32_t _event_queue_max_size;
  static uint32_t _priority_age_threshold;

  const std::string _name;
  std::shared_ptr<engine> _engine;
  const std::string _queue_file_name;
  multiplexing::muxer_filter _read_filter;
  multiplexing::muxer_filter _write_filter;
  std::string _read_filters_str;
  std::string _write_filters_str;
  const bool _persistent;

  std::shared_ptr<data_handler> _data_handler;
  std::atomic_bool _reader_running = false;

  /** Events are stacked into the queues of the three levels, or into _file
   * when the queues are full. Because several threads access to them, they
   * are protected by _events_m. */
  mutable absl::Mutex _events_m;
  std::array<level_queue, nb_levels> _queues ABSL_GUARDED_BY(_events_m);
  /* The levels of the read events, in read order. ack_events() uses it to
   * acknowledge the events in the order they were read, whatever their level.
   * For each level, the counts sum up to the `read` of its queue. */
  std::deque<read_run> _read_order ABSL_GUARDED_BY(_events_m);
  std::unique_ptr<persistent_file> _file ABSL_GUARDED_BY(_events_m);
  absl::CondVar _no_event_cv;

  std::shared_ptr<stats::center> _center;
  std::time_t _last_stats;

  /* The map of running muxers with the mutex to protect it. */
  static absl::Mutex _running_muxers_m;
  static absl::flat_hash_map<std::string, std::weak_ptr<muxer>> _running_muxers
      ABSL_GUARDED_BY(_running_muxers_m);

  /* The logger of the muxer. */
  std::shared_ptr<spdlog::logger> _logger;

  void _clean() ABSL_EXCLUSIVE_LOCKS_REQUIRED(_events_m);
  void _get_event_from_file(std::shared_ptr<io::data>& event)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(_events_m);
  void _push_to_queue(const std::shared_ptr<io::data>& event, int64_t now)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(_events_m);
  void _push_to_queue_from_file(std::shared_ptr<io::data> event)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(_events_m);
  void _demote_stale_current_events() noexcept
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(_events_m);
  void _mark_read(level lvl, size_t count)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(_events_m);
  const std::shared_ptr<io::data>* _read_next()
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(_events_m);

  size_t _unacked_size() const noexcept
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(_events_m) {
    size_t retval = 0;
    for (const level_queue& q : _queues)
      retval += q.read;
    return retval;
  }
  size_t _total_queue_size() const noexcept
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(_events_m) {
    size_t retval = 0;
    for (const level_queue& q : _queues)
      retval += q.events.size();
    return retval;
  }
  bool _has_events_to_read() const noexcept
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(_events_m) {
    for (const level_queue& q : _queues)
      if (q.unread() > 0)
        return true;
    return false;
  }

  void _update_stats(void) noexcept ABSL_EXCLUSIVE_LOCKS_REQUIRED(_events_m);

  muxer(std::string name,
        const std::shared_ptr<engine>& parent,
        const muxer_filter& r_filter,
        const muxer_filter& w_filter,
        bool persistent = false);
  void _execute_reader_if_needed();

 public:
  static std::string queue_file(const std::string& name);
  static std::string memory_file(const std::string& name);
  static void event_queue_max_size(uint32_t max) noexcept;
  static uint32_t event_queue_max_size() noexcept;
  static void priority_age_threshold(uint32_t seconds) noexcept;
  static uint32_t priority_age_threshold() noexcept;

  static std::shared_ptr<muxer> create(std::string name,
                                       const std::shared_ptr<engine>& parent,
                                       const muxer_filter& r_filter,
                                       const muxer_filter& w_filter,
                                       bool persistent = false);
  muxer(const muxer&) = delete;
  muxer& operator=(const muxer&) = delete;
  ~muxer() noexcept;
  void ack_events(uint32_t count);
  void publish(const std::deque<std::shared_ptr<io::data>>& event);
  bool read(std::shared_ptr<io::data>& event, time_t deadline) override;
  template <class container>
  bool read(container& to_fill, size_t max_to_read) noexcept
      ABSL_LOCKS_EXCLUDED(_events_m);
  const std::string& read_filters_as_str() const;
  const std::string& write_filters_as_str() const;
  uint32_t get_event_queue_size() const ABSL_LOCKS_EXCLUDED(_events_m);
  void nack_events() ABSL_LOCKS_EXCLUDED(_events_m)
      ABSL_LOCKS_EXCLUDED(_events_m);
  void remove_queue_files();
  void statistics(nlohmann::json& tree) const override
      ABSL_LOCKS_EXCLUDED(_events_m);
  void wake();
  uint32_t write(std::shared_ptr<io::data> const& d) override;
  void write(std::deque<std::shared_ptr<io::data>>& to_publish);
  uint32_t stop() override;
  const std::string& name() const;
  void set_read_filter(const muxer_filter& w_filter);
  void set_write_filter(const muxer_filter& w_filter);
  void clear_read_handler();
  void unsubscribe();
  void set_action_on_new_data(const std::shared_ptr<data_handler>& handler)
      ABSL_LOCKS_EXCLUDED(_events_m);
  void clear_action_on_new_data() ABSL_LOCKS_EXCLUDED(_events_m);
};

/**
 * @brief Read up to max_to_read events into to_fill, draining P then C then H.
 *
 * @param to_fill     Container filled via push_back.
 * @param max_to_read Maximum number of events to return.
 * @return true if there are still events to read after this call.
 */
template <class container>
bool muxer::read(container& to_fill, size_t max_to_read) noexcept {
  _logger->debug("muxer::read ({}) call", _name);
  absl::MutexLock lck(&_events_m);
  _demote_stale_current_events();
  // P, then C, then H: copy the unread events of each level in one go.
  for (uint8_t lvl = 0; lvl < nb_levels && max_to_read > 0; ++lvl) {
    level_queue& q = _queues[lvl];
    const size_t count = std::min(max_to_read, q.unread());
    if (count == 0)
      continue;
    const auto first = q.events.cbegin() + q.read;
    const auto last = first + count;
    for (auto it = first; it != last; ++it)
      to_fill.push_back(it->event);
    _mark_read(static_cast<level>(lvl), count);
    max_to_read -= count;
  }

  _update_stats();
  bool has_more = _has_events_to_read();
  if (!has_more)
    _logger->debug("muxer::read ({}) no more data to handle", _name);
  else
    _logger->debug(
        "muxer::read ({}) still some data but max count of data reached",
        _name);
  return has_more;
}

}  // namespace com::centreon::broker::multiplexing

#endif  // !CCB_MULTIPLEXING_MUXER_HH
