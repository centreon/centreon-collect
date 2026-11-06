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
#include "broker/core/bbdo/broker_stream.hh"
#include "bbdo/bbdo/ack.hh"
#include "bbdo/bbdo/stop.hh"
#include "bbdo/bbdo/version_response.hh"
#include "com/centreon/broker/multiplexing/publisher.hh"

using com::centreon::exceptions::msg_fmt;

namespace com::centreon::broker::bbdo {

namespace {
/**
 * @brief The configuration version a DiffState brings the poller to.
 *
 * A difference carries it in config_version, a whole configuration in its
 * embedded state.
 *
 * @param diff The DiffState.
 *
 * @return The version, empty if the DiffState names none.
 */
const std::string& diff_version(
    const com::centreon::engine::configuration::DiffState& diff) {
  if (diff.has_config_version())
    return diff.config_version();
  return diff.state().config_version();
}
}  // namespace

uint32_t broker_stream::stop() {
  uint32_t retval = stream::stop();
  if (poller_id() && !broker_name().empty() && !poller_name().empty())
    _state.remove_peer(poller_id(), poller_name(), broker_name());
  return retval;
}

/**
 * @brief Add the runtime snapshot of the poller's resources to a DiffState
 * about to be sent to it: what replaces retention.dat at the poller's startup
 * in centralized configuration. Engine applies it once the objects of the
 * diff exist. Nothing is added when this Broker holds no poller configuration
 * (relay) -- its cache knows no resource to describe.
 *
 * @param diff The DiffState, whose poller_id says which poller.
 */
void broker_stream::_add_runtime_state(
    com::centreon::engine::configuration::DiffState& diff) {
  if (!_state.supports_centralized_conf() || diff.unknown())
    return;
  _state.cache().fill_runtime_state(diff.poller_id(),
                                    diff.mutable_runtime_state());
  SPDLOG_LOGGER_INFO(_logger,
                     "BBDO: runtime state of {} hosts and {} services added to "
                     "the DiffState of poller {}",
                     diff.runtime_state().hosts_size(),
                     diff.runtime_state().services_size(), diff.poller_id());
}

/**
 * @brief Send the DiffState for a given poller to the stream. The DiffState is
 * read from the file system, where it was stored by the configuration applier
 * when the Engine requested an update.
 *
 * A file that cannot be parsed is not sent: Engine would refuse it and close
 * the connection, and the poller would never apply its configuration. It stays
 * owed, and the next read of the stream tries again.
 *
 * The file holds the difference from `<ID>.prot`, which is what the cache and
 * the database need. A poller that does not run that version (it lost its
 * state.prot, for instance) cannot apply it: it is sent the full configuration
 * read from `new-<ID>.prot` instead. The choice is made here and not when the
 * diff is prepared, as the poller may have reconnected with another version in
 * between.
 *
 * @param poller_id The ID of the poller for which to send the DiffState.
 */
void broker_stream::_send_diff_state_for_poller(uint64_t poller_id) {
  auto pb_conf = std::make_shared<pb_diff_state>();
  auto& obj = pb_conf->mut_obj();
  std::filesystem::path diff_name(_state.pollers_config_dir() /
                                  fmt::format("diff-{}.prot", poller_id));
  std::ifstream f(diff_name, std::ios::binary);
  if (!f) {
    _logger->error("BBDO: failed to open diff file '{}' for poller {}",
                   diff_name.string(), poller_id);
    return;
  }
  if (!obj.ParseFromIstream(&f)) {
    _logger->error(
        "BBDO: cannot parse diff file '{}' for poller {}: not sent, it stays "
        "owed",
        diff_name.string(), poller_id);
    return;
  }
  f.close();
  if (!obj.has_state() && obj.has_from_version()) {
    const std::string engine_conf = _state.poller_engine_conf(poller_id);
    if (engine_conf != obj.from_version()) {
      std::filesystem::path new_name(_state.pollers_config_dir() /
                                     fmt::format("new-{}.prot", poller_id));
      auto full =
          std::make_unique<com::centreon::engine::configuration::State>();
      std::ifstream nf(new_name, std::ios::binary);
      if (!nf || !full->ParseFromIstream(&nf)) {
        _logger->error(
            "BBDO: poller {} runs configuration '{}' and not '{}' the diff "
            "starts from, but '{}' cannot be read: nothing sent, it stays owed",
            poller_id, engine_conf, obj.from_version(), new_name.string());
        return;
      }
      SPDLOG_LOGGER_INFO(_logger,
                         "BBDO: poller {} runs configuration '{}' and not '{}' "
                         "the diff starts from: sending it the full "
                         "configuration '{}'",
                         poller_id, engine_conf, obj.from_version(),
                         full->config_version());
      obj.Clear();
      obj.set_poller_id(static_cast<uint32_t>(poller_id));
      obj.set_allocated_state(full.release());
    }
  }
  /* Read before _add_runtime_state(), which only adds the snapshot. */
  const std::string version = diff_version(obj);
  _add_runtime_state(obj);
  SPDLOG_LOGGER_INFO(_logger,
                     "BBDO: sending DiffState to poller {} (unknown={})",
                     poller_id, obj.unknown());
  _write(pb_conf);
  _state.set_poller_conf_sent(static_cast<uint32_t>(poller_id), version);
}

/**
 * @brief Handle a BBDO event. Events of category io::bbdo are the guardians
 * of BBDO messages. These messages are used by the protocol itself and are
 * always prioritized.
 *
 * @param d The event to handle.
 */
void broker_stream::_handle_bbdo_event(const std::shared_ptr<io::data>& d) {
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
    case pb_welcome::static_type(): {
      auto welcome(std::static_pointer_cast<pb_welcome>(d));
      const auto& pb_version = welcome->obj().version();
      if (pb_version.major() != get_bbdo_version().major_v) {
        SPDLOG_LOGGER_ERROR(
            _logger,
            "BBDO: peer is using protocol version {}.{}.{}, whereas we're "
            "using protocol version {}.{}.{}",
            pb_version.major(), pb_version.minor(), pb_version.patch(),
            get_bbdo_version().major_v, get_bbdo_version().minor_v,
            get_bbdo_version().patch);
        throw msg_fmt(
            "BBDO: peer is using protocol version {}.{}.{} "
            "whereas we're using protocol version {}.{}.{}",
            pb_version.major(), pb_version.minor(), pb_version.patch(),
            get_bbdo_version().major_v, get_bbdo_version().minor_v,
            get_bbdo_version().patch);
      }
      SPDLOG_LOGGER_INFO(
          _logger,
          "BBDO: peer is using protocol version {}.{}.{} , we're using "
          "version "
          "{}.{}.{}",
          pb_version.major(), pb_version.minor(), pb_version.patch(),
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
    case pb_diff_state_ack::static_type(): {
      auto ack_ptr = std::static_pointer_cast<pb_diff_state_ack>(d);
      auto& obj = ack_ptr->obj();
      if (_state.is_relay()) {
        // Relay: forward the ack upstream to the central.
        SPDLOG_LOGGER_INFO(
            _logger,
            "BBDO: relay queuing DiffStateAck for central for poller {}",
            obj.poller_id());
        _state.push_pending_diff_state_ack(ack_ptr);
        break;
      }
      // Central: handle the ack (direct connection or forwarded by a relay).
      if (!obj.failed_version().empty()) {
        SPDLOG_LOGGER_INFO(_logger,
                           "BBDO: received diff state failure from poller {} "
                           "for version '{}', still running '{}'",
                           obj.poller_id(), obj.failed_version(),
                           obj.config_version());
        _state.on_poller_conf_rejected(obj.poller_id(), obj.config_version(),
                                       obj.failed_version(), obj.error());
        break;
      }
      SPDLOG_LOGGER_INFO(
          _logger,
          "BBDO: received diff state ack from poller {} with version '{}'",
          obj.poller_id(), obj.config_version());
      _state.on_poller_conf_acknowledged(obj.poller_id(),
                                         obj.config_version());
    } break;
    case pb_diff_state::static_type(): {
      auto diff = std::static_pointer_cast<bbdo::pb_diff_state>(d);
      if (!diff->obj().unknown() && _state.supports_centralized_conf()) {
        // Central: Engine sent its full state back → store as N.prot.
        assert(diff->obj().has_state());
        _state.create_prot_file(diff->obj().state());
      } else if (_state.is_relay()) {
        // Relay: received DiffState from central → queue for forwarding to
        // the Engine-connected stream.  No local storage.
        uint32_t pid = diff->obj().poller_id();
        if (pid == 0 && diff->obj().has_state())
          pid = diff->obj().state().poller_id();
        SPDLOG_LOGGER_INFO(
            _logger,
            "BBDO: relay received DiffState from central for poller {} "
            "(unknown={})",
            pid, diff->obj().unknown());
        if (pid > 0)
          _state.push_pending_diff_state(pid, diff);
        else
          _logger->error(
              "BBDO: relay received DiffState with unknown poller_id");
      }
    } break;
    case pb_config_request::static_type(): {
      auto req = std::static_pointer_cast<pb_config_request>(d);
      const uint64_t engine_id = req->obj().poller_id();
      const std::string& engine_name = req->obj().poller_name();
      const std::string& version = req->obj().config_version();
      SPDLOG_LOGGER_INFO(_logger,
                         "BBDO: received ConfigRequest from relay for poller "
                         "{} (version '{}')",
                         engine_id, version);

      _state.register_poller_via_relay(engine_id, engine_name, poller_id(),
                                       version);

      using response_t = config::applier::broker_state::relay_config_response;
      switch (_state.prepare_relay_config_response(engine_id, version)) {
        case response_t::unknown: {
          SPDLOG_LOGGER_INFO(
              _logger,
              "BBDO: central has no config for relay poller {} — sending "
              "unknown DiffState",
              engine_id);
          auto diff_state = std::make_shared<pb_diff_state>();
          diff_state->mut_obj().set_unknown(true);
          diff_state->mut_obj().set_poller_id(static_cast<uint32_t>(engine_id));
          _write(diff_state);
          break;
        }
        case response_t::diff_ready:
          _send_diff_state_for_poller(engine_id);
          break;
        case response_t::up_to_date: {
          // Engine already has the latest config. Per protocol, send an empty
          // DiffState so Engine can complete the handshake with DiffStateAck.
          SPDLOG_LOGGER_INFO(
              _logger,
              "BBDO: relay already has latest config for poller {} — sending "
              "empty DiffState for handshake",
              engine_id);
          auto diff_state = std::make_shared<pb_diff_state>();
          diff_state->mut_obj().set_poller_id(static_cast<uint32_t>(engine_id));
          _add_runtime_state(diff_state->mut_obj());
          _write(diff_state);
          _state.set_poller_conf_sent(static_cast<uint32_t>(engine_id),
                                      version);
          break;
        }
      }
    } break;
    case pb_config_revoke::static_type(): {
      auto rev = std::static_pointer_cast<pb_config_revoke>(d);
      const uint64_t pid = rev->obj().poller_id();
      SPDLOG_LOGGER_INFO(_logger, "BBDO: received ConfigRevoke for poller {}",
                         pid);
      _state.clear_pending_for_poller(pid);
    } break;
    default:
      break;
  }
}

/**
 *  Read data from stream.
 *
 *  @param[out] d         Next available event.
 *  @param[in]  deadline  Deadline.
 *
 *  @return Respect io::stream::read() return value.
 *
 *  @see input::read()
 */
bool broker_stream::read(std::shared_ptr<io::data>& d, time_t deadline) {
  bool retval = stream::read(d, deadline);
  if (peer_type() == common::ENGINE)
    _serve_engine_peer();
  else if (peer_type() == common::BROKER)
    _serve_broker_peer();
  return retval;
}

/**
 * @brief What an Engine peer is owed: its configuration when a new one is
 * ready, the runtime snapshot of its resources once per connection, the
 * events queued for it on the downward channel, and, on a relay, the
 * DiffState forwarded by the central.
 */
void broker_stream::_serve_engine_peer() {
  /* The runtime snapshot is owed to the poller from its connection on, and
   * again once Broker rebuilt a configuration it had lost. It travels with
   * the configuration diff when there is one, alone otherwise. */
  if (_state.poller_needs_update(poller_id())) {
    _logger->debug(
        "BBDO: We should send the Engine configuration to the poller {}",
        poller_id());
    _send_diff_state_for_poller(poller_id());
    _state.set_runtime_state_owed(poller_id(), false);
  } else if (_state.runtime_state_owed(poller_id()) &&
             extended_negotiation() && _state.supports_centralized_conf() &&
             _state.broker_knows_poller_conf(poller_id()) &&
             _state.cache().has_instance(poller_id())) {
    /* has_instance: a configuration just rebuilt from the poller reaches the
     * cache later, through the reference output, which sets the flag again
     * once there is something to describe. */
    _send_runtime_state_alone();
    _state.set_runtime_state_owed(poller_id(), false);
  }

  /* Downward channel: deliver the events queued for the poller this stream
   * supervises (notification executions, external commands). */
  for (auto& evt : _state.pop_pending_for_poller(poller_id())) {
    SPDLOG_LOGGER_DEBUG(_logger, "BBDO: sending event 0x{:x} to poller {}",
                        evt->type(), poller_id());
    _write(evt);
  }

  // Relay ENGINE stream: forward any pending DiffState from the central.
  if (_state.is_relay()) {
    auto diff = _state.pop_pending_diff_state_for_engine(poller_id());
    if (diff) {
      SPDLOG_LOGGER_INFO(
          _logger, "BBDO: relay forwarding DiffState to Engine for poller {}",
          poller_id());
      _write(diff);
    }
  }
}

/**
 * @brief The poller runs the configuration Broker knows: no diff to send, but
 * the runtime snapshot of its resources still is -- what replaces
 * retention.dat at its startup. A DiffState that carries nothing else, flagged
 * runtime_only, with the version the poller already runs so that its
 * acknowledgement changes nothing. Not a configuration round: nothing is
 * marked as sent, the acknowledgement only confirms the version.
 */
void broker_stream::_send_runtime_state_alone() {
  auto diff_state = std::make_shared<pb_diff_state>();
  auto& obj = diff_state->mut_obj();
  obj.set_poller_id(static_cast<uint32_t>(poller_id()));
  obj.set_config_version(_state.poller_engine_conf(poller_id()));
  obj.set_runtime_only(true);
  _add_runtime_state(obj);
  SPDLOG_LOGGER_INFO(
      _logger,
      "BBDO: poller {} is up to date, sending it its runtime state alone",
      poller_id());
  _write(diff_state);
}

/**
 * @brief What a Broker peer is owed. On the central, the peer is a relay:
 * the ConfigRevoke messages of the migration path and the DiffStates of the
 * Engines behind it. On a relay, the peer is the upstream central: the
 * ConfigRequests and DiffStateAcks of the Engines the relay serves.
 */
void broker_stream::_serve_broker_peer() {
  if (!_state.is_relay()) {
    for (uint64_t engine_id : _state.pop_pending_config_revokes(poller_id())) {
      auto rev = std::make_shared<pb_config_revoke>();
      rev->mut_obj().set_poller_id(static_cast<uint32_t>(engine_id));
      SPDLOG_LOGGER_INFO(_logger,
                         "BBDO: sending ConfigRevoke to relay for poller {}",
                         engine_id);
      _write(rev);
    }
    for (uint64_t engine_id :
         _state.pollers_via_relay_needing_update(poller_id()))
      _send_diff_state_for_poller(engine_id);
  } else {
    for (auto& [pid, poller_name, version] :
         _state.pop_pending_config_requests()) {
      auto req = std::make_shared<pb_config_request>();
      auto& obj = req->mut_obj();
      obj.set_poller_id(pid);
      obj.set_poller_name(poller_name);
      obj.set_config_version(version);
      SPDLOG_LOGGER_INFO(_logger,
                         "BBDO: relay sending ConfigRequest to upstream for "
                         "poller {} (version '{}')",
                         pid, version);
      _write(req);
    }
    for (auto& ack : _state.pop_pending_diff_state_acks()) {
      const auto& ack_obj =
          std::static_pointer_cast<pb_diff_state_ack>(ack)->obj();
      SPDLOG_LOGGER_INFO(
          _logger,
          "BBDO: relay forwarding DiffStateAck to central for poller {}",
          ack_obj.poller_id());
      _write(ack);
    }
  }
}

/**
 * @brief Returns true if this stream supports centralized configuration.
 * This is the case when Broker has a cache configuration directory set.
 *
 * @return true if centralized configuration is supported, false otherwise.
 */
bool broker_stream::supports_centralized_conf() const {
  return !_state.cache_config_dir().empty();
}

/**
 * @brief Fill the Welcome message with broker-specific negotiation parameters.
 * Sets the extended negotiation flag if centralized configuration is supported,
 * and tells Engine whether Broker owns the notification decision so Engine can
 * stop deciding notifications on its own (notification_mode=broker).
 *
 * @param obj The Welcome message to fill.
 */
void broker_stream::specific_negotiate(Welcome& obj) {
  obj.set_extended_negotiation(supports_centralized_conf());
  obj.set_broker_handles_notifications(_state.notifications_on_broker());
}

}  // namespace com::centreon::broker::bbdo
