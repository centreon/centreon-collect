/**
 * Copyright 2009-2017-2023 Centreon
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
class muxer : public io::stream {
 public:
  using read_handler = std::function<void()>;

 private:
  static uint32_t _event_queue_max_size;

  const std::string _name;
  std::shared_ptr<engine> _engine;
  const std::string _queue_file_name;
  multiplexing::muxer_filter _read_filter;
  multiplexing::muxer_filter _write_filter;
  std::string _read_filters_str;
  std::string _write_filters_str;
  const bool _persistent;
  read_handler _read_handler;

  std::unique_ptr<persistent_file> _file;
  std::condition_variable _cv;
  mutable std::mutex _mutex;
  std::list<std::shared_ptr<io::data>> _events;
  size_t _events_size;
  std::list<std::shared_ptr<io::data>>::iterator _pos;
  std::time_t _last_stats;

  static std::mutex _running_muxers_m;
  static absl::flat_hash_map<std::string, std::weak_ptr<muxer>> _running_muxers;

  void _clean();
  void _get_event_from_file(std::shared_ptr<io::data>& event);
  void _push_to_queue(std::shared_ptr<io::data> const& event);

  void _update_stats(void) noexcept;

  muxer(std::string name,
        std::unique_ptr<engine>& parent,
        const muxer_filter& r_filter,
        const muxer_filter& w_filter,
        bool persistent = false);

 public:
  static std::string queue_file(const std::string& name);
  static std::string memory_file(const std::string& name);
  static void event_queue_max_size(uint32_t max) noexcept;
  static uint32_t event_queue_max_size() noexcept;

  static std::shared_ptr<muxer> create(std::string name,
                                       std::unique_ptr<engine>& parent,
                                       const muxer_filter& r_filter,
                                       const muxer_filter& w_filter,
                                       bool persistent = false);
  muxer(const muxer&) = delete;
  muxer& operator=(const muxer&) = delete;
  ~muxer() noexcept;
  void ack_events(int count);
  void publish(const std::deque<std::shared_ptr<io::data>>& event);
  bool read(std::shared_ptr<io::data>& event, time_t deadline) override;
  template <class container>
  bool read(container& to_fill,
            size_t max_to_read,
            read_handler&& handler) noexcept;
  const std::string& read_filters_as_str() const;
  const std::string& write_filters_as_str() const;
  uint32_t get_event_queue_size() const;
  void nack_events();
  void remove_queue_files();
  void statistics(nlohmann::json& tree) const override;
  void wake();
  int32_t write(std::shared_ptr<io::data> const& d) override;
  void write(std::deque<std::shared_ptr<io::data>>& to_publish);
  int32_t stop() override;
  const std::string& name() const;
  void set_read_filter(const muxer_filter& w_filter);
  void set_write_filter(const muxer_filter& w_filter);
  void clear_read_handler();
  void unsubscribe();
};

/**
 * @brief read data from queue and store in to_fill with the push_back method
 * It tries to write max_to_read events in to_fill. If there are no more
 * events in the queue, it stores handler and will call it as soon as events are
 * pushed in queue (publish only not transferred from retention to queue)
 *
 * @tparam container list<std::shared_ptr<io::data>> or
 * vector<std::shared_ptr<io::data>>
 * @param to_fill container to  fill
 * @param max_to_read max events to read from muxer and to push_back in to_fill
 * @param handler handler that will be called if we can't read max_to_read
 * events as soon as data will be available
 */
template <class container>
bool muxer::read(container& to_fill,
                 size_t max_to_read,
                 read_handler&& handler) noexcept {
  std::unique_lock<std::mutex> lock(_mutex);

  size_t nb_read = 0;
  while (_pos != _events.end() && nb_read < max_to_read) {
    to_fill.push_back(*_pos);
    ++_pos;
    ++nb_read;
  }
  // no more data => store handler to call when data will be available
  if (_pos == _events.end()) {
    _read_handler = std::move(handler);
    _update_stats();
    return false;
  } else {
    _update_stats();
    return true;
  }
}

}  // namespace com::centreon::broker::multiplexing

#endif  // !CCB_MULTIPLEXING_MUXER_HH
