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
#include <absl/time/time.h>

#include "bbdo/bbdo/ack.hh"
#include "bbdo/bbdo/stop.hh"
#include "bbdo/bbdo/version_response.hh"
#include "broker/core/bbdo/cbmod_stream.hh"
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
    case version_response::static_type(): {
      auto version(std::static_pointer_cast<version_response>(d));
      if (version->bbdo_major != get_bbdo_version().major_v) {
        SPDLOG_LOGGER_ERROR(
            _logger,
            "BBDO: peer is using protocol version {}.{}.{}, whereas we're "
            "using protocol version {}.{}.{}",
            version->bbdo_major, version->bbdo_minor, version->bbdo_patch,
            get_bbdo_version().major_v, get_bbdo_version().minor_v,
            get_bbdo_version().patch);
        throw msg_fmt(
            "BBDO: peer is using protocol version {}.{}.{} "
            "whereas we're using protocol version {}.{}.{}",
            version->bbdo_major, version->bbdo_minor, version->bbdo_patch,
            get_bbdo_version().major_v, get_bbdo_version().minor_v,
            get_bbdo_version().patch);
      }
      SPDLOG_LOGGER_INFO(
          _logger,
          "BBDO: peer is using protocol version {}.{}.{} , we're using "
          "version "
          "{}.{}.{}",
          version->bbdo_major, version->bbdo_minor, version->bbdo_patch,
          get_bbdo_version().major_v, get_bbdo_version().minor_v,
          get_bbdo_version().patch);

      break;
    }
    case ack::static_type():
      SPDLOG_LOGGER_INFO(
          _logger, "BBDO: received acknowledgement for {} events",
          std::static_pointer_cast<const ack>(d)->acknowledged_events);
      acknowledge_events(
          std::static_pointer_cast<const ack>(d)->acknowledged_events);
      break;
    case pb_ack::static_type():
      SPDLOG_LOGGER_INFO(_logger,
                         "BBDO: received pb acknowledgement for {} events",
                         std::static_pointer_cast<const pb_ack>(d)
                             ->obj()
                             .acknowledged_events());
      acknowledge_events(std::static_pointer_cast<const pb_ack>(d)
                             ->obj()
                             .acknowledged_events());
      break;
    case stop::static_type(): {
      SPDLOG_LOGGER_INFO(_logger, "BBDO: received stop from peer");
      send_event_acknowledgement();
    } break;
    case pb_stop::static_type(): {
      SPDLOG_LOGGER_INFO(
          _logger, "BBDO: received stop from peer with ID {}",
          std::static_pointer_cast<pb_stop>(d)->obj().poller_id());
      send_event_acknowledgement();
      /* Now, we send a local::pb_stop to ask unified_sql to update the
       * database since the poller is going away. */
      auto loc_stop = std::make_shared<local::pb_stop>();
      auto& obj = loc_stop->mut_obj();
      obj.set_poller_id(
          std::static_pointer_cast<pb_stop>(d)->obj().poller_id());
      multiplexing::publisher pblshr;
      pblshr.write(loc_stop);
    } break;
    case pb_diff_state::static_type():
      _state.set_diff_state(d);
      break;
    case pb_notification_execute::static_type():
      /* notification_mode=broker: Broker made the notification decision and
       * dispatched the execution here. Queue it for the Engine event loop,
       * which runs the notification commands (macros + notify_contact). A
       * notification for a resource this poller does not supervise is dropped
       * later by the loop when the resource is not found. */
      _state.push_notification_execute(d);
      break;
    default:
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
  /* Advertise the poller machine's local timezone so Broker can evaluate
   * notification timeperiods in the poller's timezone when a host/service has
   * no explicit timezone directive (which resolves, on Engine, to the machine's
   * local timezone). */
  obj.set_timezone(absl::LocalTimeZone().name());
}

/**
 * @brief Extract cbmod-specific negotiation parameters from the peer's Welcome.
 * If Broker advertised that it owns the notification decision
 * (notification_mode=broker), Engine must stop deciding notifications on its
 * own; it will only execute the notifications Broker dispatches.
 *
 * @param peer The Welcome message received from Broker.
 */
void cbmod_stream::specific_negotiate_received(const Welcome& peer) {
  if (peer.broker_handles_notifications()) {
    SPDLOG_LOGGER_INFO(
        _logger,
        "BBDO: Broker handles notifications; Engine notification decision "
        "disabled");
    _state.set_broker_handles_notifications(true);
  }
}

}  // namespace com::centreon::broker::bbdo
