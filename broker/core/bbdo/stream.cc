/**
 * Copyright 2013,2015,2017, 2021-2026 Centreon
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

#include "broker/core/bbdo/stream.hh"

#include <arpa/inet.h>

#include "bbdo/bbdo/ack.hh"
#include "bbdo/bbdo/stop.hh"
#include "bbdo/bbdo/version_response.hh"
#include "broker/core/config/applier/state.hh"
#include "com/centreon/broker/exceptions/timeout.hh"
#include "com/centreon/broker/io/protocols.hh"
#include "com/centreon/broker/misc/misc.hh"
#include "com/centreon/broker/multiplexing/publisher.hh"

using namespace com::centreon::exceptions;
using namespace com::centreon::broker;

using com::centreon::common::log_v2::log_v2;

namespace com::centreon::broker::bbdo {

/**
 * @brief Construct a new stream::stream object
 *
 * @param is_input true if we receive bbdo events such as broker input
 * @param grpc_serialized true if serialization is done by grpc stream only
 * @param extensions
 */
stream::stream(bool is_input,
               bool grpc_serialized,
               const std::list<std::shared_ptr<io::extension>>& extensions)
    : basic_stream(is_input, grpc_serialized),
      _negotiate{true},
      _negotiated{false},
      _extensions{extensions} {
  SPDLOG_LOGGER_DEBUG(log_v2::instance().get(log_v2::CORE),
                      "create bbdo stream {:p}",
                      static_cast<const void*>(this));
}

/**
 *  Negotiate features with peer.
 *
 *  @param[in] neg  Negotiation type.
 */
void stream::negotiate(stream::negotiation_type neg) {
  SPDLOG_LOGGER_TRACE(_logger, "BBDO: negotiate");
  std::string extensions;
  if (!_negotiate) {
    SPDLOG_LOGGER_INFO(_logger, "BBDO: negotiation disabled.");
    extensions = _get_extension_names(true);
  } else
    extensions = _get_extension_names(false);

  bbdo::bbdo_version v300(3, 0, 0);
  const bbdo::bbdo_version& my_bbdo_version = get_bbdo_version();

  // Send our own packet if we should be first.
  if (neg == negotiate_first) {
    SPDLOG_LOGGER_DEBUG(
        _logger, "BBDO: sending welcome packet (available extensions: {})",
        extensions);
    /* if _negotiate, we send all the extensions we would like to have,
     * otherwise we only send the mandatory extensions */
    if (my_bbdo_version.total_version <= v300.total_version) {
      auto welcome_packet{
          std::make_shared<version_response>(my_bbdo_version, extensions)};
      _write(welcome_packet);
    } else {
      auto welcome{std::make_shared<pb_welcome>()};
      auto& obj = welcome->mut_obj();
      obj.mutable_version()->set_major(my_bbdo_version.major_v);
      obj.mutable_version()->set_minor(my_bbdo_version.minor_v);
      obj.mutable_version()->set_patch(my_bbdo_version.patch);
      obj.set_extensions(extensions);
      obj.set_poller_id(config::applier::state::instance().poller_id());
      obj.set_poller_name(config::applier::state::instance().poller_name());
      obj.set_broker_name(config::applier::state::instance().broker_name());
      obj.set_peer_type(config::applier::state::instance().peer_type());
      /* If I'm Engine or Broker, I have some specific negotiation to do. */
      specific_negotiate(obj);

      _write(welcome);
    }
  }

  // Read peer packet.
  SPDLOG_LOGGER_DEBUG(_logger, "BBDO: retrieving welcome packet of peer");
  std::shared_ptr<io::data> d;
  time_t deadline;
  if (timeout() == (time_t)-1)
    deadline = (time_t)-1;
  else
    deadline = time(nullptr) + timeout();

  _read_any(d, deadline);
  /* Unexpected or missing response. */
  if (!d || (d->type() != version_response::static_type() &&
             d->type() != pb_welcome::static_type())) {
    std::string msg;
    if (d)
      msg = fmt::format(
          "BBDO: invalid protocol header, aborting connection: waiting for "
          "message of type '{}' but received type is {}",
          my_bbdo_version.total_version > v300.total_version
              ? "pb_welcome"
              : "version_response",
          d->type());
    else
      msg = fmt::format(
          "BBDO: invalid protocol header, aborting connection: waiting for "
          "message of type '{}' but nothing received",
          my_bbdo_version.total_version > v300.total_version
              ? "pb_welcome"
              : "version_response");
    SPDLOG_LOGGER_ERROR(_logger, msg);
    throw msg_fmt(msg);
  }

  std::string peer_extensions;
  std::string peer_engine_conf;
  _extended_negotiation = false;

  if (d->type() == version_response::static_type()) {
    std::shared_ptr<version_response> v(
        std::static_pointer_cast<version_response>(d));
    if (v->bbdo_major != my_bbdo_version.major_v) {
      SPDLOG_LOGGER_ERROR(
          _logger,
          "BBDO: peer is using protocol version {}.{}.{} whereas we're using "
          "protocol version {}.{}.{}",
          v->bbdo_major, v->bbdo_minor, v->bbdo_patch, my_bbdo_version.major_v,
          my_bbdo_version.minor_v, my_bbdo_version.patch);
      throw msg_fmt(
          "BBDO: peer is using protocol version {}.{}.{}"
          " whereas we're using protocol version {}.{}.{}",
          v->bbdo_major, v->bbdo_minor, v->bbdo_patch, my_bbdo_version.major_v,
          my_bbdo_version.minor_v, my_bbdo_version.patch);
    }
    SPDLOG_LOGGER_INFO(
        _logger,
        "BBDO: peer is using protocol version {}.{}.{}, we're using version "
        "{}.{}.{}",
        v->bbdo_major, v->bbdo_minor, v->bbdo_patch, my_bbdo_version.major_v,
        my_bbdo_version.minor_v, my_bbdo_version.patch);

    // Send our own packet if we should be second.
    if (neg == negotiate_second) {
      SPDLOG_LOGGER_DEBUG(
          _logger, "BBDO: sending welcome packet (available extensions: {})",
          extensions);
      /* if _negotiate, we send all the extensions we would like to have,
       * otherwise we only send the mandatory extensions */
      auto welcome_packet(
          std::make_shared<version_response>(my_bbdo_version, extensions));
      _write(welcome_packet);
      _substream->flush();
    }
    peer_extensions = v->extensions;
  } else {
    const auto& w = std::static_pointer_cast<pb_welcome>(d)->obj();
    _logger->trace("BBDO: received pb_welcome packet: {}",
                   w.ShortDebugString());
    const auto& pb_version = w.version();
    if (pb_version.major() != my_bbdo_version.major_v) {
      SPDLOG_LOGGER_ERROR(
          _logger,
          "BBDO: peer is using protocol version {}.{}.{} whereas we're using "
          "protocol version {}.{}.{}",
          pb_version.major(), pb_version.minor(), pb_version.patch(),
          my_bbdo_version.major_v, my_bbdo_version.minor_v,
          my_bbdo_version.patch);
      throw msg_fmt(
          "BBDO: peer is using protocol version {}.{}.{}"
          " whereas we're using protocol version {}.{}.{}",
          pb_version.major(), pb_version.minor(), pb_version.patch(),
          my_bbdo_version.major_v, my_bbdo_version.minor_v,
          my_bbdo_version.patch);
    }
    SPDLOG_LOGGER_INFO(
        _logger,
        "BBDO: peer is using protocol version {}.{}.{}, we're using version "
        "{}.{}.{}",
        pb_version.major(), pb_version.minor(), pb_version.patch(),
        my_bbdo_version.major_v, my_bbdo_version.minor_v,
        my_bbdo_version.patch);

    // Send our own packet if we should be second.
    if (neg == negotiate_second) {
      SPDLOG_LOGGER_DEBUG(
          _logger, "BBDO: sending welcome packet (available extensions: {})",
          extensions);
      auto welcome = std::make_shared<pb_welcome>();
      auto& obj = welcome->mut_obj();
      obj.mutable_version()->set_major(std::min(
          my_bbdo_version.major_v, static_cast<uint16_t>(w.version().major())));
      obj.mutable_version()->set_minor(std::min(
          my_bbdo_version.minor_v, static_cast<uint16_t>(w.version().minor())));
      obj.mutable_version()->set_patch(std::min(
          my_bbdo_version.patch, static_cast<uint16_t>(w.version().patch())));
      obj.set_extensions(extensions);
      obj.set_poller_id(config::applier::state::instance().poller_id());
      obj.set_poller_name(config::applier::state::instance().poller_name());
      obj.set_broker_name(config::applier::state::instance().broker_name());
      obj.set_peer_type(config::applier::state::instance().peer_type());
      /* If I'm Engine or Broker, I have some specific negotiation to do. */
      specific_negotiate(obj);

      _write(welcome);
      _substream->flush();
    }
    peer_engine_conf = w.engine_conf();
    peer_extensions = w.extensions();
    set_poller_id(w.poller_id());
    set_poller_name(w.poller_name());
    set_broker_name(w.broker_name());

    set_peer_type(w.peer_type());
    if (peer_type() != common::UNKNOWN) {
      /* We are in the bbdo stream, _poller_id, _broker_name,
       * _extended_negotiation are informations about the peer, not us. */
      _extended_negotiation = true;
    }
  }

  // Negotiation.
  std::list<std::string> running_config = get_running_config();

  // Apply negotiated extensions.
  SPDLOG_LOGGER_INFO(_logger, "BBDO: we have extensions '{}' and peer has '{}'",
                     extensions, peer_extensions);
  std::list<std::string_view> peer_ext{absl::StrSplit(peer_extensions, ' ')};
  for (auto& ext : _extensions) {
    // Find matching extension in peer extension list.
    auto peer_it{std::find(peer_ext.begin(), peer_ext.end(), ext->name())};
    // Apply extension if found.
    if (peer_it != peer_ext.end()) {
      if (std::find(running_config.begin(), running_config.end(),
                    ext->name()) == running_config.end()) {
        SPDLOG_LOGGER_INFO(_logger, "BBDO: applying extension '{}'",
                           ext->name());
        for (absl::btree_map<std::string,
                             io::protocols::protocol>::const_iterator
                 proto_it = io::protocols::instance().begin(),
                 proto_end = io::protocols::instance().end();
             proto_it != proto_end; ++proto_it) {
          if (absl::EqualsIgnoreCase(proto_it->first, ext->name())) {
            std::shared_ptr<io::stream> s{
                proto_it->second.endpntfactry->new_stream(
                    _substream, neg == negotiate_second, ext->options())};
            set_substream(s);
            break;
          }
        }
      } else
        SPDLOG_LOGGER_INFO(_logger, "BBDO: extension '{}' already configured",
                           ext->name());
    } else {
      if (ext->is_mandatory()) {
        SPDLOG_LOGGER_ERROR(
            _logger,
            "BBDO: extension '{}' is set to 'yes' in the configuration but "
            "cannot be activated because of peer configuration.",
            ext->name());
      }
      if (std::find(running_config.begin(), running_config.end(),
                    ext->name()) != running_config.end()) {
        SPDLOG_LOGGER_INFO(_logger, "BBDO: extension '{}' no more needed",
                           ext->name());
        auto substream = get_substream();
        if (substream->get_name() == ext->name()) {
          auto subsubstream = substream->get_substream();
          set_substream(subsubstream);
        } else {
          while (substream) {
            auto parent = substream;
            substream = substream->get_substream();
            if (substream->get_name() == ext->name()) {
              parent->set_substream(substream->get_substream());
              break;
            }
          }
        }
      }
    }
  }

  // Stream has now negotiated.
  _negotiated = true;
  /* With old BBDO, we don't have poller_id nor poller name available. */
  if (poller_id() > 0 && !broker_name().empty()) {
    _logger->debug("Adding peer {}:{}:{} with version '{}'", poller_id(),
                   poller_name(), broker_name(), peer_engine_conf);
    config::applier::state::instance().add_peer(
        poller_id(), poller_name(), broker_name(), peer_type(),
        _extended_negotiation, peer_engine_conf);
    if (!config::applier::state::instance().is_peer_conf_known(poller_id())) {
      _logger->error("No known configuration for the poller {}:{}:{}",
                     poller_id(), poller_name(), broker_name());
      /* We send an unknown diff state to let the peer know that its
       * configuration is not known by Broker. */
      if (_extended_negotiation && peer_type() == common::ENGINE &&
          supports_centralized_conf()) {
        _logger->debug(
            "Sending unknown diff state to peer {}:{}:{} to let it know that "
            "its configuration is not known by Broker.",
            poller_id(), poller_name(), broker_name());
        auto diff_state = std::make_shared<bbdo::pb_diff_state>();
        diff_state->mut_obj().set_unknown(true);
        _write(diff_state);
      }
    }
  }
  SPDLOG_LOGGER_TRACE(_logger, "Negotiation done.");
}

std::string stream::_get_extension_names(bool mandatory) const {
  std::string retval;
  if (mandatory)
    for (auto& e : _extensions) {
      if (e->is_mandatory()) {
        if (!retval.empty())
          retval.append(" ");
        retval.append(e->name());
      }
    }
  else
    for (auto& e : _extensions) {
      if (e->is_optional() || e->is_mandatory()) {
        if (!retval.empty())
          retval.append(" ");
        retval.append(e->name());
      }
    }
  return retval;
}

void stream::set_negotiate(bool negotiate) {
  _negotiate = negotiate;
}

}  // namespace com::centreon::broker::bbdo
