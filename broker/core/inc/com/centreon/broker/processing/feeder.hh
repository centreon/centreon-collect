/*
** Copyright 2011-2012, 2020-2023 Centreon
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

#ifndef CCB_PROCESSING_FEEDER_HH
#define CCB_PROCESSING_FEEDER_HH

#include <climits>

#include "com/centreon/broker/multiplexing/muxer.hh"
#include "com/centreon/broker/processing/stat_visitable.hh"

namespace com::centreon::broker {

// Forward declaration.
namespace io {
class stream;
}

namespace processing {
/**
 *  @class feeder feeder.hh "com/centreon/broker/processing/feeder.hh"
 *  @brief Feed events from a source to a destination.
 *
 *  Take events from a source and send them to a destination.
 */
class feeder : public stat_visitable,
               public std::enable_shared_from_this<feeder> {
  enum class state : unsigned { running, finished };
  // Condition variable used when waiting for the thread to finish
  std::atomic<state> _state;

  std::shared_ptr<io::stream> _client;
  // as the muxer may be embeded in a lambda run by asio thread, we use a
  // shared_ptr
  std::shared_ptr<multiplexing::muxer> _muxer;

  asio::system_timer _stat_timer;
  asio::system_timer _read_from_stream_timer;
  std::shared_ptr<asio::io_context> _io_context;

  mutable std::timed_mutex _protect;

 protected:
  feeder(const std::string& name,
         const std::shared_ptr<multiplexing::engine>& parent,
         std::shared_ptr<io::stream>& client,
         const multiplexing::muxer_filter& read_filters,
         const multiplexing::muxer_filter& write_filters);

  const std::string& _get_read_filters() const override;
  const std::string& _get_write_filters() const override;
  void _forward_statistic(nlohmann::json& tree) override;
  uint32_t _get_queued_events() const override;

  void _start_stat_timer();
  void _stat_timer_handler(const boost::system::error_code& err);

  void _start_read_from_stream_timer();
  void _read_from_stream_timer_handler(const boost::system::error_code& err);

  void _read_from_muxer();
  unsigned _write_to_client(
      const std::vector<std::shared_ptr<io::data>>& events);

  void _stop_no_lock();

  void _ack_event_to_muxer(unsigned count) noexcept;

 public:
  static std::shared_ptr<feeder> create(
      const std::string& name,
      const std::shared_ptr<multiplexing::engine>& parent,
      std::shared_ptr<io::stream>& client,
      const multiplexing::muxer_filter& read_filters,
      const multiplexing::muxer_filter& write_filters);

  ~feeder();
  feeder(const feeder&) = delete;
  feeder& operator=(const feeder&) = delete;

  void stop();

  bool is_finished() const noexcept;

  bool wait_for_all_events_written(unsigned ms_timeout);
};
}  // namespace processing

}

#endif  // !CCB_PROCESSING_FEEDER_HH
