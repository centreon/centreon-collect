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
#include "common/engine_conf/indexed_diff_state.hh"

using com::centreon::exceptions::msg_fmt;

namespace com::centreon::broker::bbdo {

int32_t broker_stream::stop() {
  int32_t retval = stream::stop();
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
      auto& obj = std::static_pointer_cast<pb_diff_state_ack>(d)->obj();
      assert(obj.poller_id() == poller_id());
      _state.set_poller_engine_conf(poller_id(), poller_name(), broker_name(),
                                    obj.config_version());
      _state.acknowledge_engine_peer(obj.poller_id());
      SPDLOG_LOGGER_INFO(
          _logger,
          "BBDO: received diff state ack from poller {} with version '{}'",
          obj.poller_id(), obj.config_version());
      std::filesystem::path new_name(_state.pollers_config_dir() /
                                     fmt::format("new-{}.prot", poller_id()));
      std::filesystem::path name(_state.pollers_config_dir() /
                                 fmt::format("{}.prot", poller_id()));
      _logger->debug("bbdo::basic_stream removing {}", name.string());
      std::error_code ec;
      std::filesystem::rename(new_name, name, ec);
      if (ec)
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

  if (peer_type() == common::ENGINE &&
      _state.engine_peer_needs_update(poller_id())) {
    _logger->debug(
        "BBDO: We should send the Engine configuration to the poller {}",
        poller_id());
    auto pb_conf = std::make_shared<pb_diff_state>();
    auto& obj = pb_conf->mut_obj();
    std::filesystem::path diff_name(_state.pollers_config_dir() /
                                    fmt::format("diff-{}.prot", poller_id()));
    std::ifstream f(diff_name);
    if (f) {
      std::error_code ec;
      obj.ParseFromIstream(&f);
      f.close();
      uint32_t id = obj.has_state() ? obj.state().poller_id() : obj.poller_id();
      _logger->debug(
          "BBDO: Sending Engine configuration to poller {} and from diff state "
          "{}",
          poller_id(), id);
      _write(pb_conf);
      _state.set_available_conf_sent_to_engine_peer(poller_id());
    }
  }
  return retval;
}

}  // namespace com::centreon::broker::bbdo
