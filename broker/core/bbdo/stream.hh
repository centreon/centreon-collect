/**
 * Copyright 2013,2017-2023 Centreon
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

#ifndef CCB_BBDO_STREAM_HH
#define CCB_BBDO_STREAM_HH

#include "bbdo/bbdo/bbdo_version.hh"
#include "bbdo/common.pb.h"
#include "com/centreon/broker/io/extension.hh"
#include "com/centreon/broker/io/raw.hh"
#include "com/centreon/broker/io/stream.hh"

namespace com::centreon::broker::bbdo {
/**
 *  @class stream stream.hh "broker/core/bbdo/stream.hh"
 *  @brief BBDO stream.
 *
 *  The class converts data to NEB events back and forth.
 *
 *  It is a little tricky around acknowledgements.
 *  This steam is able to read, to write and to flush.
 *
 *  * write() serializes an event and writes it to the substream. It returns how
 * many events can be acknowledged. But this count is not directly accessible,
 * it comes from the ack message sent by the peer. So we do not have to count
 * how many events are serialized, sometimes, we get an ack message and here is
 * the value.
 *  * read() gets some buffer from the substream and unserializes it to create
 * an event. The internal buffer is probably not empty after a call to read
 * since buffers are not synchronous with events.
 *
 *
 *  There are also three variables to manage acknowledgements:
 *  * _events_received_since_last_ack: It is incremented each time a data is
 * read. If this value is equal to the _ack_limit, an ack message is sent to the
 * peer and this value is reset to 0. When the peer receives this ack message,
 * it releases the corresponding events.
 *  * _acknowledged_events: represents the number of events correctly received
 * by the peer after calls to write().
 */
class stream : public io::stream {
  class buffer {
    uint32_t _event_id;
    uint32_t _source_id;
    uint32_t _dest_id;

    /* All the read data are get in vectors. We chose to not cut those vectors
     * and just move them into the deque. */
    std::deque<std::vector<char>> _buf;

   public:
    buffer(uint32_t event_id,
           uint32_t source_id,
           uint32_t dest_id,
           std::vector<char>&& v)
        : _event_id(event_id), _source_id(source_id), _dest_id(dest_id) {
      _buf.push_back(v);
    }
    buffer(const buffer&) = delete;
    buffer(buffer&& other)
        : _event_id(other._event_id),
          _source_id(other._source_id),
          _dest_id(other._dest_id),
          _buf(std::move(other._buf)) {}

    buffer& operator=(const buffer&) = delete;
    buffer& operator=(buffer&& other) {
      if (this != &other) {
        _event_id = other._event_id;
        _source_id = other._source_id;
        _dest_id = other._dest_id;
        _buf = std::move(other._buf);
      }
      return *this;
    }
    ~buffer() noexcept = default;

    bool matches(uint32_t event_id,
                 uint32_t source_id,
                 uint32_t dest_id) const noexcept {
      return event_id == _event_id && source_id == _source_id &&
             dest_id == _dest_id;
    }

    std::vector<char> to_vector() {
      size_t s = 0;
      for (auto& v : _buf)
        s += v.size();
      std::vector<char> retval;
      retval.reserve(s);
      for (auto& v : _buf)
        retval.insert(retval.end(), v.begin(), v.end());
      _buf.clear();
      return retval;
    }

    void push_back(std::vector<char>&& v) { _buf.push_back(v); }
    uint32_t get_event_id() const { return _event_id; }
  };

  /* input */
  /* If during a packet reading, we get several ones, this vector is useful
   * to keep in cache all but the first one. It will be read before a call
   * to _read_packet(). */
  std::vector<char> _packet;

  /* We could get parts of BBDO packets in the wrong order, this deque is useful
   * to paste parts together in the good order. */
  std::deque<buffer> _buffer;
  int32_t _skipped;

  // void _buffer_must_have_unprocessed(int bytes, time_t deadline =
  // (time_t)-1);
  void _read_packet(size_t size, time_t deadline = (time_t)-1);

  bool _is_input;
  bool _coarse;
  bool _negotiate;
  bool _negotiated;
  int _timeout;
  uint32_t _acknowledged_events;
  uint32_t _ack_limit;
  uint32_t _events_received_since_last_ack;
  time_t _last_sent_ack;
  const bool _grpc_serialized;

  /**
   * @brief when we bypass bbdo serialization, we store object received by grpc
   * layer in this queue
   *
   */
  std::deque<std::shared_ptr<io::data>> _grpc_serialized_queue;

  /**
   * It is possible to mix bbdo stream with others like tls or compression.
   * This list of extensions provides a simple access to others ones with
   * their configuration.
   */
  std::list<std::shared_ptr<io::extension>> _extensions;
  bbdo::bbdo_version _bbdo_version;

  /* bbdo logger */
  std::shared_ptr<spdlog::logger> _logger;

  void _write(std::shared_ptr<io::data> const& d);
  bool _read_any(std::shared_ptr<io::data>& d, time_t deadline);
  void _handle_bbdo_event(const std::shared_ptr<io::data>& d);
  bool _wait_for_bbdo_event(uint32_t expected_type,
                            std::shared_ptr<io::data>& d,
                            time_t deadline);
  void _send_event_stop_and_wait_for_ack();
  std::string _get_extension_names(bool mandatory) const;
  /* Poller Name of the peer: used since BBDO 3.0.1 */
  std::string _poller_name;
  /* Broker Name of the peer: used since BBDO 3.0.1 */
  std::string _broker_name;
  /* ID of the peer poller: used since BBDO 3.0.1 */
  uint64_t _poller_id = 0u;
  /* True if the peer supports extended negotiation */
  bool _extended_negotiation = false;
  /* Type of the peer: used since BBDO 3.0.1 */
  common::PeerType _peer_type = common::UNKNOWN;
  /* Currently, this is a hash of the Engine configuration directory. It's
   * filled when neb::pb_instance is sent to Broker. */
  std::string _config_version;

  io::data* unserialize(uint32_t event_type,
                        uint32_t source_id,
                        uint32_t destination_id,
                        const char* buffer,
                        uint32_t size);
  io::raw* serialize(const io::data& e);

 public:
  enum negotiation_type { negotiate_first = 1, negotiate_second, negotiated };

  stream(bool is_input,
         bool grpc_serialized = false,
         const std::list<std::shared_ptr<io::extension>>& extensions = {});
  ~stream();
  stream(const stream&) = delete;
  stream& operator=(const stream&) = delete;
  int32_t stop() override;
  int flush() override;
  void negotiate(negotiation_type neg);
  bool read(std::shared_ptr<io::data>& d,
            time_t deadline = (time_t)-1) override;
  void set_ack_limit(uint32_t limit);
  void set_coarse(bool coarse);
  void set_negotiate(bool negotiate);
  void set_timeout(int timeout);
  void statistics(nlohmann::json& tree) const override;
  int write(std::shared_ptr<io::data> const& d) override;
  void acknowledge_events(uint32_t events);
  void send_event_acknowledgement();
  std::list<std::string> get_running_config();
  bool check_poller_configuration(uint64_t poller_id,
                                  const std::string& expected_version);
};
}  // namespace com::centreon::broker::bbdo

#endif  // !CCB_BBDO_STREAM_HH
