/**
 * Copyright 2013-2015,2017 Centreon
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

#include "acceptor.hh"

#include <cassert>

#include "com/centreon/broker/config/applier/init.hh"
#include "com/centreon/broker/io/events.hh"
#include "com/centreon/broker/io/protocols.hh"
#include "com/centreon/broker/multiplexing/muxer_filter.hh"
#include "internal.hh"
#include "stream.hh"

using namespace com::centreon::broker;
using namespace com::centreon::broker::bbdo;

/**
 *  Constructor.
 *
 *  @param[in] name                    The name to build temporary.
 *  @param[in] negotiate               true if feature negotiation is
 *                                     allowed.
 *  @param[in] extensions              Pair of two strings with extensions the
 *                                     one contains those allowed by this
 *                                     endpoint. The second one contains the
 *                                     mandatory ones.
 *  @param[in] timeout                 Connection timeout.
 *  @param[in] one_peer_retention_mode True to enable the "one peer
 *                                     retention mode" (TM).
 *  @param[in] coarse                  If the acceptor is coarse or not.
 *  @param[in] ack_limit               The number of event received before
 *                                     an ack needs to be sent.
 */
acceptor::acceptor(std::string name,
                   bool negotiate,
                   time_t timeout,
                   bool one_peer_retention_mode,
                   bool coarse,
                   uint32_t ack_limit,
                   std::list<std::shared_ptr<io::extension>>&& extensions,
                   bool grpc_serialized)
    : io::endpoint(
          !one_peer_retention_mode,
          multiplexing::muxer_filter(multiplexing::muxer_filter::zero_init()),
          multiplexing::muxer_filter(multiplexing::muxer_filter::zero_init())),
      _coarse(coarse),
      _name(std::move(name)),
      _negotiate(negotiate),
      _is_output(one_peer_retention_mode),
      _timeout(timeout),
      _ack_limit(ack_limit),
      _extensions{extensions},
      _grpc_serialized(grpc_serialized) {
  if (_timeout == (time_t)-1 || _timeout == 0)
    _timeout = 3;
}

/**
 *  Destructor.
 */
acceptor::~acceptor() noexcept {
  _from.reset();
}

/**
 *  Wait for incoming connection.
 *
 *  @return Always return null stream. A new thread will be launched to
 *          process the incoming connection.
 */
std::shared_ptr<io::stream> acceptor::open() {
  // Wait for client from the lower layer.
  if (_from) {
    std::shared_ptr<io::stream> u = _from->open();

    // Add BBDO layer.
    if (u) {
      assert(!_coarse);
      // if _is_output, the stream is an output
      auto my_bbdo = std::make_unique<bbdo::stream>(
          !_is_output, _grpc_serialized, _extensions);
      my_bbdo->set_substream(u);
      my_bbdo->set_coarse(_coarse);
      my_bbdo->set_negotiate(_negotiate);
      my_bbdo->set_timeout(_timeout);
      my_bbdo->set_ack_limit(_ack_limit);
      try {
        my_bbdo->negotiate(bbdo::stream::negotiate_second);
      } catch (const std::exception& e) {
        u->stop();
        throw;
      }

      return my_bbdo;
    }
  }

  return std::shared_ptr<io::stream>();
}

/**
 *  Get BBDO statistics.
 *
 *  @param[out] tree Properties tree.
 */
void acceptor::stats(nlohmann::json& tree) {
  tree["one_peer_retention_mode"] = _is_output;
  if (_from)
    _from->stats(tree);
}
