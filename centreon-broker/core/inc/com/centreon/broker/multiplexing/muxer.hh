/*
** Copyright 2009-2017-2021 Centreon
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
#include <unordered_set>

#include "com/centreon/broker/multiplexing/engine.hh"
#include "com/centreon/broker/persistent_file.hh"

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
  using filters = std::unordered_set<uint32_t>;

 private:
  std::condition_variable _cv;
  std::list<std::shared_ptr<io::data>> _events;
  static uint32_t _event_queue_max_size;
  std::unique_ptr<persistent_file> _file;
  bool _queue_file_enabled;
  std::string _queue_file_name;
  mutable std::mutex _mutex;
  std::string _name;
  const filters _read_filters;
  const filters _write_filters;
  const std::string _read_filters_str;
  const std::string _write_filters_str;
  bool _persistent;
  std::list<std::shared_ptr<io::data>>::iterator _pos;

  void _clean();
  void _get_event_from_file(std::shared_ptr<io::data>& event);
  std::string _memory_file() const;
  void _push_to_queue(std::shared_ptr<io::data> const& event);

  void updateStats(void) noexcept;

  MuxerStats* _stats;
  std::time_t _clk;

 public:
  muxer(std::string name,
        muxer::filters r_filters,
        muxer::filters w_filters,
        bool persistent = false);
  muxer(const muxer&) = delete;
  muxer& operator=(const muxer&) = delete;
  ~muxer() noexcept;
  void ack_events(int count);
  static void event_queue_max_size(uint32_t max) noexcept;
  static uint32_t event_queue_max_size() noexcept;
  void publish(std::shared_ptr<io::data> const event);
  bool read(std::shared_ptr<io::data>& event, time_t deadline) override;
  const filters& read_filters() const;
  const filters& write_filters() const;
  const std::string& read_filters_as_str() const;
  const std::string& write_filters_as_str() const;
  uint32_t get_event_queue_size() const;
  void nack_events();
  void remove_queue_files();
  void statistics(nlohmann::json& tree) const override;
  void wake();
  int32_t write(std::shared_ptr<io::data> const& d) override;
  int32_t stop() override;
  const std::string& name() const;

  static std::string memory_file(std::string const& name);
  static std::string queue_file(std::string const& name);
};
}  // namespace multiplexing

CCB_END()

#endif  // !CCB_MULTIPLEXING_MUXER_HH
