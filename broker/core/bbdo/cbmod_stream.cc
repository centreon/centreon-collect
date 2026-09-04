/**
 * Copyright 2026 Centreon
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
#include "broker/core/bbdo/cbmod_stream.hh"
#include "bbdo/bbdo/version_response.hh"
#include "broker/core/bbdo/basic_stream.hh"
#include "com/centreon/broker/multiplexing/publisher.hh"

using com::centreon::exceptions::msg_fmt;

namespace com::centreon::broker::bbdo {

/**
 * @brief Send the Engine configuration to the connected Broker peer.
 * The configuration is sent as a pb_diff_state message containing the full
 * Engine state, in response to a DiffState{unknown=true} request from Broker.
 *
 * @param conf The Engine configuration state to send.
 */
void cbmod_stream::send_engine_conf(
    std::unique_ptr<com::centreon::engine::configuration::State>& conf) {
  auto pb_conf = std::make_shared<bbdo::pb_diff_state>();
  auto& obj = pb_conf->mut_obj();
  obj.set_allocated_state(conf.release());
  _write(pb_conf);
}

uint32_t cbmod_stream::write(const std::shared_ptr<io::data>& d) {
  if (_state.peer_type() == common::ENGINE && peer_type() == common::BROKER) {
    if (_state.diff_state_applied()) {
      const std::string& version = _state.engine_conf();
      _logger->debug("BBDO: Sending diff state '{}' acknowledgement", version);
      auto diff_state_ack = std::make_shared<bbdo::pb_diff_state_ack>();
      auto& obj = diff_state_ack->mut_obj();
      obj.set_poller_id(_state.poller_id());
      obj.set_config_version(version);
      _write(diff_state_ack);
    }
    if (auto current_conf = _state.current_engine_conf(); current_conf) {
      _logger->debug("BBDO: Sending current engine configuration '{}' to peer",
                     current_conf->config_version());
      send_engine_conf(current_conf);
    }
  }

  return stream::write(d);
}

uint32_t cbmod_stream::stop() {
  uint32_t retval = stream::stop();
  if (poller_id() && !broker_name().empty() && !poller_name().empty())
    _state.remove_peer(poller_id(), poller_name(), broker_name());
  return retval;
}

/**
 * @brief Handle a BBDO event. Events of category io::bbdo are the guardians
 * of BBDO messages. These messages are used by the protocol itself and are
 * always prioritized.
 *
 * @param d The event to handle.
 */
void cbmod_stream::_handle_bbdo_event(const std::shared_ptr<io::data>& d) {
  switch (d->type()) {
    case pb_diff_state::static_type():
      _state.set_diff_state(d);
      break;
    default:
      basic_stream::_handle_bbdo_event(d);
      break;
  }
}

/**
 * @brief Returns true if this stream supports centralized configuration.
 * This is the case when a proto configuration file (state.prot) is present.
 *
 * @return true if centralized configuration is supported, false otherwise.
 */
bool cbmod_stream::supports_centralized_conf() const {
  return !_state.proto_conf().empty();
}

/**
 * @brief Fill the Welcome message with cbmod-specific negotiation parameters.
 * Sets the extended negotiation flag and the current Engine configuration
 * version if centralized configuration is supported.
 *
 * @param obj The Welcome message to fill.
 */
void cbmod_stream::specific_negotiate(Welcome& obj) {
  if (supports_centralized_conf()) {
    obj.set_extended_negotiation(true);
    obj.set_engine_conf(_state.engine_conf());
  }
}

}  // namespace com::centreon::broker::bbdo
