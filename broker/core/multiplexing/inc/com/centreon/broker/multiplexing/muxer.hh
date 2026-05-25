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

  struct queue_entry {
    int64_t timestamp;               // INT64_MAX for P, insertion time for C/H
    std::shared_ptr<io::data> event; // nullptr if tombstoned (C only)
  };

 private:
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

  /** Events are stacked into three priority queues or into _file. Because
   * several threads access to them, they are protected by _events_m.
   *   P (_priority_events)   — pb_downtime, pb_acknowledgement
   *   C (_current_events)    — fresh host/service status events
   *   H (_historical_events) — bulk data, logs, old status events
   * Read order: P first, then C (skipping tombstones), then H. */
  mutable absl::Mutex _events_m;
  std::deque<queue_entry> _priority_events ABSL_GUARDED_BY(_events_m);
  std::deque<queue_entry> _current_events ABSL_GUARDED_BY(_events_m);
  std::deque<queue_entry> _historical_events ABSL_GUARDED_BY(_events_m);
  size_t _priority_read_pos ABSL_GUARDED_BY(_events_m) = 0;
  size_t _current_read_pos ABSL_GUARDED_BY(_events_m) = 0;
  size_t _historical_read_pos ABSL_GUARDED_BY(_events_m) = 0;
  size_t _current_events_size ABSL_GUARDED_BY(_events_m) = 0;
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
  void _push_to_queue(std::shared_ptr<io::data> const& event)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(_events_m);
  void _push_to_queue_from_file(std::shared_ptr<io::data> event)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(_events_m);
  void _demote_stale_current_events() noexcept
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(_events_m);

  size_t _total_queue_size() const noexcept
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(_events_m) {
    return _priority_events.size() + _current_events_size +
           _historical_events.size();
  }
  bool _has_events_to_read() const noexcept
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(_events_m) {
    return _priority_read_pos < _priority_events.size() ||
           _current_read_pos < _current_events.size() ||
           _historical_read_pos < _historical_events.size();
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
  size_t nb_read = 0;

  // Queue P
  while (_priority_read_pos < _priority_events.size() && nb_read < max_to_read) {
    to_fill.push_back(_priority_events[_priority_read_pos++].event);
    ++nb_read;
  }
  // Queue C (skip tombstones)
  while (_current_read_pos < _current_events.size() && nb_read < max_to_read) {
    auto& entry = _current_events[_current_read_pos++];
    if (entry.event) {
      to_fill.push_back(entry.event);
      ++nb_read;
    }
  }
  // Queue H
  while (_historical_read_pos < _historical_events.size() &&
         nb_read < max_to_read) {
    to_fill.push_back(_historical_events[_historical_read_pos++].event);
    ++nb_read;
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
