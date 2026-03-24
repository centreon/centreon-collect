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

#include "broker/core/bbdo/basic_stream.hh"

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
class stream : public basic_stream {
  /* True if the peer supports extended negotiation */
  bool _extended_negotiation = false;
  bool _negotiate;
  bool _negotiated;
  /**
   * It is possible to mix bbdo stream with others like tls or compression.
   * This list of extensions provides a simple access to others ones with
   * their configuration.
   */
  std::list<std::shared_ptr<io::extension>> _extensions;

  std::string _get_extension_names(bool mandatory) const;

 public:
  enum negotiation_type { negotiate_first = 1, negotiate_second, negotiated };

  stream(bool is_input,
         bool grpc_serialized = false,
         const std::list<std::shared_ptr<io::extension>>& extensions = {});
  void negotiate(negotiation_type neg);
  virtual bool supports_centralized_conf() const = 0;
  virtual void specific_negotiate(Welcome& obj) = 0;
  void set_negotiate(bool negotiate);
};
}  // namespace com::centreon::broker::bbdo

#endif  // !CCB_BBDO_STREAM_HH
