/*
** Copyright 2009-2017 Centreon
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
**
** For more information : contact@centreon.com
*/

#ifndef CCB_MULTIPLEXING_MUXER_HH
#define CCB_MULTIPLEXING_MUXER_HH

#include <condition_variable>
#include <list>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <unordered_set>
#include <chrono>

#include "com/centreon/broker/namespace.hh"
#include "com/centreon/broker/persistent_file.hh"
#include "com/centreon/broker/multiplexing/engine.hh"

CCB_BEGIN()

namespace multiplexing {
/**
 *  @class muxer muxer.hh "com/centreon/broker/multiplexing/muxer.hh"
 *  @brief Receive and send events from/to the multiplexing engine.
 *
 *  This class is a cornerstone in event multiplexing. Each endpoint
 *  willing to communicate with the multiplexing engine will create a
 *  new muxer object. This objects broadcast events sent to it to all
 *  other muxer objects.
 *
 *  @see engine
 */
class muxer : public io::stream {
 public:
  typedef std::unordered_set<uint32_t> filters;

 private:
  std::condition_variable _cv;
  std::list<std::shared_ptr<io::data> > _events;
  uint32_t _events_size;
  static uint32_t _event_queue_max_size;
  std::unique_ptr<persistent_file> _file;
  bool _queue_file_enabled;
  std::string _queue_file_name;
  mutable std::mutex _mutex;
  std::string _name;
  bool _persistent;
  std::list<std::shared_ptr<io::data> >::iterator _pos;
  filters _read_filters;
  filters _write_filters;
  std::string _read_filters_str;
  std::string _write_filters_str;

  void _clean();
  void _get_event_from_file(std::shared_ptr<io::data>& event);
  std::string _memory_file() const;
  void _push_to_queue(std::shared_ptr<io::data> const& event);

  void updateStats(void) noexcept {
    auto now(std::chrono::system_clock::now());
    if (std::chrono::duration_cast<std::chrono::milliseconds>(now - _clk)
            .count() > 1000) {
      _clk = now;
      _stats->set_queue_file_enabled(_queue_file_enabled);
      _stats->set_queue_file(_queue_file_name);
      _stats->set_unacknowledged_events(std::to_string(_events_size));
    }
  }

  MuxerStats* _stats;
  std::chrono::time_point<std::chrono::system_clock> _clk;

 public:
  muxer(std::string const& name, bool persistent = false);
  muxer(const muxer&) = delete;
  muxer& operator=(const muxer&) = delete;
  ~muxer() noexcept;
  void ack_events(int count);
  static void event_queue_max_size(uint32_t max) noexcept;
  static uint32_t event_queue_max_size() noexcept;
  void publish(std::shared_ptr<io::data> const event);
  bool read(std::shared_ptr<io::data>& event, time_t deadline) override;
  void set_read_filters(filters const& fltrs);
  void set_write_filters(filters const& fltrs);
  filters const& get_read_filters() const;
  filters const& get_write_filters() const;
  const std::string& get_read_filters_str() const;
  const std::string& get_write_filters_str() const;
  uint32_t get_event_queue_size() const;
  void nack_events();
  void remove_queue_files();
  void statistics(nlohmann::json& tree);
  void wake();
  int32_t write(std::shared_ptr<io::data> const& d) override;
  int32_t stop() override;

  static std::string memory_file(std::string const& name);
  static std::string queue_file(std::string const& name);
};
}  // namespace multiplexing

CCB_END()

#endif  // !CCB_MULTIPLEXING_MUXER_HH
