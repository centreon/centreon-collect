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
#include "broker/core/bbdo/basic_stream.hh"
#include "com/centreon/broker/multiplexing/publisher.hh"
#include "common/engine_conf/indexed_diff_state.hh"

using com::centreon::exceptions::msg_fmt;

namespace com::centreon::broker::bbdo {

uint32_t broker_stream::stop() {
  uint32_t retval = stream::stop();
  if (poller_id() && !broker_name().empty() && !poller_name().empty())
    _state.remove_peer(poller_id(), poller_name(), broker_name());
  return retval;
}

/**
 * @brief Send the DiffState for a given poller to the stream. The DiffState is
 * read from the file system, where it was stored by the configuration applier
 * when the Engine requested an update.
 *
 * @param poller_id The ID of the poller for which to send the DiffState.
 */
void broker_stream::_send_diff_state_for_poller(uint64_t poller_id) {
  auto pb_conf = std::make_shared<pb_diff_state>();
  auto& obj = pb_conf->mut_obj();
  std::filesystem::path diff_name(_state.pollers_config_dir() /
                                  fmt::format("diff-{}.prot", poller_id));
  std::ifstream f(diff_name);
  if (f) {
    obj.ParseFromIstream(&f);
    f.close();
    SPDLOG_LOGGER_INFO(_logger,
                       "BBDO: sending DiffState to poller {} (unknown={})",
                       poller_id, obj.unknown());
    _write(pb_conf);
    _state.set_available_conf_sent_to_engine_peer(
        static_cast<uint32_t>(poller_id));
  } else {
    _logger->error("BBDO: failed to open diff file '{}' for poller {}",
                   diff_name.string(), poller_id);
  }
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
      const uint64_t engine_id = obj.poller_id();
      _state.set_poller_engine_conf(engine_id, obj.config_version());
      _state.acknowledge_engine_peer(engine_id);
      SPDLOG_LOGGER_INFO(
          _logger,
          "BBDO: received diff state ack from poller {} with version '{}'",
          engine_id, obj.config_version());
      std::filesystem::path new_name(_state.pollers_config_dir() /
                                     fmt::format("new-{}.prot", engine_id));
      std::filesystem::path name(_state.pollers_config_dir() /
                                 fmt::format("{}.prot", engine_id));
      _logger->debug("bbdo::basic_stream removing {}", name.string());
      std::error_code ec;
      std::filesystem::rename(new_name, name, ec);
      if (ec && ec != std::errc::no_such_file_or_directory)
        _logger->error("Unable to rename the file from '{}' to '{}'",
                       new_name.string(), name.string());

      // All the peer pollers have their configuration acknowledged.
      if (_state.all_engine_peers_acknowledged()) {
        SPDLOG_LOGGER_INFO(
            _logger,
            "BBDO: all engine peers have acknowledged their configuration");
        com::centreon::engine::configuration::indexed_diff_state global_diff;
        std::error_code ec;
        for (const auto& entry : std::filesystem::directory_iterator(
                 _state.pollers_config_dir(), ec)) {
          std::string poller_id_str(entry.path().filename().string());
          if (entry.is_regular_file() && entry.path().extension() == ".prot" &&
              absl::StartsWith(poller_id_str, "diff-")) {
            _logger->debug("BBDO: Merging diff file '{}' into the global one",
                           entry.path().string());
            std::string_view poller_id_view(poller_id_str);
            poller_id_view.remove_prefix(5);
            poller_id_view.remove_suffix(5);
            uint64_t poller_id;
            if (absl::SimpleAtoi(poller_id_view, &poller_id)) {
              std::filesystem::path diff_name(_state.pollers_config_dir() /
                                              entry.path());
              std::ifstream f(diff_name);
              com::centreon::engine::configuration::DiffState diff;
              if (f) {
                diff.ParseFromIstream(&f);
                f.close();
                global_diff.add_diff_state(diff, _logger);
                _logger->debug("BBDO: Removing diff file '{}'",
                               diff_name.string());
                std::filesystem::remove(diff_name);
              }
            } else {
              _logger->error(
                  "BBDO: The file '{}' seems not to be a diff state file.",
                  poller_id_str);
            }
          }
        }
        auto diff = std::make_shared<neb::pb_global_diff_state>();
        auto& obj = diff->mut_obj();
        global_diff.release_diff_state(obj);
        multiplexing::publisher pblshr;
        _logger->debug("BBDO: Publishing global diff state");
        pblshr.write(diff);
      }
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

      _state.register_engine_peer_via_relay(engine_id, engine_name, poller_id(),
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
          _write(diff_state);
          _state.set_available_conf_sent_to_engine_peer(
              static_cast<uint32_t>(engine_id));
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
      basic_stream::_handle_bbdo_event(d);
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

  if (peer_type() == common::ENGINE &&
      _state.engine_peer_needs_update(poller_id())) {
    _logger->debug(
        "BBDO: We should send the Engine configuration to the poller {}",
        poller_id());
    _send_diff_state_for_poller(poller_id());
  }

  // Relay ENGINE stream: forward any pending DiffState from the central.
  if (peer_type() == common::ENGINE && _state.is_relay()) {
    auto diff = _state.pop_pending_diff_state_for_engine(poller_id());
    if (diff) {
      SPDLOG_LOGGER_INFO(
          _logger, "BBDO: relay forwarding DiffState to Engine for poller {}",
          poller_id());
      _write(diff);
    }
  }

  if (peer_type() == common::BROKER) {
    if (!_state.is_relay()) {
      // Central: send pending ConfigRevoke messages to this relay (migration
      // path).
      for (uint64_t engine_id :
           _state.pop_pending_config_revokes(poller_id())) {
        auto rev = std::make_shared<pb_config_revoke>();
        rev->mut_obj().set_poller_id(static_cast<uint32_t>(engine_id));
        SPDLOG_LOGGER_INFO(_logger,
                           "BBDO: sending ConfigRevoke to relay for poller {}",
                           engine_id);
        _write(rev);
      }
      // Central: for each engine peer behind this relay that has a pending PHP
      // config update, push the DiffState to the relay.
      for (uint64_t engine_id :
           _state.engine_peers_via_relay_needing_update(poller_id())) {
        _send_diff_state_for_poller(engine_id);
      }
    }
    /* When this broker is a relay (no pollers_config_dir), forward any pending
     * ConfigRequests and DiffStateAcks to the upstream central Broker. */
    else {
      for (auto& [pid, poller_name, version] :
           _state.pop_pending_config_requests()) {
        auto req = std::make_shared<pb_config_request>();
        auto& obj = req->mut_obj();
        obj.set_poller_id(pid);
        obj.set_poller_name(poller_name);
        obj.set_config_version(version);
        SPDLOG_LOGGER_INFO(
            _logger,
            "BBDO: relay sending ConfigRequest to upstream for poller {} "
            "(version '{}')",
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

  return retval;
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
 * Sets the extended negotiation flag if centralized configuration is supported.
 *
 * @param obj The Welcome message to fill.
 */
void broker_stream::specific_negotiate(Welcome& obj) {
  obj.set_extended_negotiation(supports_centralized_conf());
}

}  // namespace com::centreon::broker::bbdo
