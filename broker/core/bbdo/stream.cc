/**
 * Copyright 2013,2015,2017, 2021-2024 Centreon
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

#include <absl/strings/str_split.h>
#include <arpa/inet.h>

#include "bbdo/bbdo/ack.hh"
#include "bbdo/bbdo/stop.hh"
#include "bbdo/bbdo/version_response.hh"
#include "com/centreon/broker/config/applier/state.hh"
#include "com/centreon/broker/exceptions/timeout.hh"
#include "com/centreon/broker/io/protocols.hh"
#include "com/centreon/broker/misc/misc.hh"
#include "com/centreon/broker/multiplexing/publisher.hh"
#include "com/centreon/broker/neb/internal.hh"
#include "com/centreon/common/file.hh"
#include "common/log_v2/log_v2.hh"

using namespace com::centreon::exceptions;
using namespace com::centreon::broker;
using namespace com::centreon::broker::bbdo;

using log_v2 = com::centreon::common::log_v2::log_v2;

/**
 *  Set a boolean within an object.
 */
static uint32_t set_boolean(io::data& t,
                            mapping::entry const& member,
                            void const* data,
                            uint32_t size) {
  if (!size)
    throw msg_fmt(
        "cannot extract boolean value: "
        "0 bytes left in packet");

  member.set_bool(t, *static_cast<char const*>(data));
  return 1;
}

/**
 *  Set a double within an object.
 */
static uint32_t set_double(io::data& t,
                           mapping::entry const& member,
                           void const* data,
                           uint32_t size) {
  char const* str(static_cast<char const*>(data));
  uint32_t len(strlen(str));
  if (len >= size)
    throw msg_fmt(
        "cannot extract double value: "
        "not terminating '\\0' in remaining {} bytes of packet",
        size);
  member.set_double(t, strtod(str, nullptr));
  return len + 1;
}

/**
 *  Set an integer within an object.
 */
static uint32_t set_integer(io::data& t,
                            mapping::entry const& member,
                            void const* data,
                            uint32_t size) {
  if (size < sizeof(uint32_t))
    throw msg_fmt(
        "BBDO: cannot extract integer value: {}"
        " bytes left in packet",
        size);
  member.set_int(t, ntohl(*static_cast<uint32_t const*>(data)));
  return sizeof(uint32_t);
}

/**
 *  Set a short within an object.
 */
static uint32_t set_short(io::data& t,
                          mapping::entry const& member,
                          void const* data,
                          uint32_t size) {
  if (size < sizeof(uint16_t))
    throw msg_fmt(
        "BBDO: cannot extract short value: {}"
        " bytes left in packet",
        size);
  member.set_short(t, ntohs(*static_cast<uint16_t const*>(data)));
  return sizeof(uint16_t);
}

/**
 *  Set a string within an object.
 */
static uint32_t set_string(io::data& t,
                           mapping::entry const& member,
                           void const* data,
                           uint32_t size) {
  char const* str(static_cast<char const*>(data));
  uint32_t len(strlen(str));
  if (len >= size)
    throw msg_fmt(
        "BBDO: cannot extract string value: "
        "no terminating '\\0' in remaining {} bytes of packet",
        size);
  member.set_string(t, str);
  return len + 1;
}

/**
 *  Set a timestamp within an object.
 */
static uint32_t set_timestamp(io::data& t,
                              mapping::entry const& member,
                              void const* data,
                              uint32_t size) {
  if (size < sizeof(uint64_t))
    throw msg_fmt(
        "BBDO: cannot extract timestamp value: {}"
        " bytes left in packet",
        size);
  uint32_t const* ptr(static_cast<uint32_t const*>(data));
  uint64_t val(ntohl(*ptr));
  ++ptr;
  val <<= 32;
  val |= ntohl(*ptr);
  member.set_time(t, val);
  return sizeof(uint64_t);
}

/**
 *  Set an uint32_teger within an object.
 */
static uint32_t set_uint(io::data& t,
                         mapping::entry const& member,
                         void const* data,
                         uint32_t size) {
  if (size < sizeof(uint32_t))
    throw msg_fmt(
        "BBDO: cannot extract uint32_teger value: {}"
        " bytes left in packet",
        size);
  member.set_uint(t, ntohl(*static_cast<uint32_t const*>(data)));
  return sizeof(uint32_t);
}

/**
 *  Set an uint64_teger within an object.
 */
static uint32_t set_ulong(io::data& t,
                          mapping::entry const& member,
                          void const* data,
                          uint32_t size) {
  if (size < sizeof(uint64_t))
    throw msg_fmt(
        "BBDO: cannot extract uint64_teger value: {}"
        " bytes left in packet",
        size);
  const uint32_t* ptr(static_cast<uint32_t const*>(data));
  uint64_t val(ntohl(*ptr));
  ++ptr;
  val <<= 32;
  val |= ntohl(*ptr);

  member.set_ulong(t, val);
  return sizeof(uint64_t);
}

/**
 *  Unserialize an event in the BBDO protocol.
 *
 *  @param[in] event_type  Event type.
 *  @param[in] source_id   The source id.
 *  @param[in] destination The destination id.
 *  @param[in] buffer      Serialized data.
 *  @param[in] size        Buffer size.
 *
 *  @return Event.
 */
io::data* stream::unserialize(uint32_t event_type,
                              uint32_t source_id,
                              uint32_t destination_id,
                              const char* buffer,
                              uint32_t size) {
  // Get event info (operations and mapping).
  io::event_info const* info(io::events::instance().get_event_info(event_type));
  if (info) {
    // Create object.
    if (info->get_mapping()) {
      std::unique_ptr<io::data> t(info->get_operations().constructor());
      if (t) {
        t->source_id = source_id;
        t->destination_id = destination_id;
        // Browse all mapping to unserialize the object.
        for (const mapping::entry* current_entry = info->get_mapping();
             !current_entry->is_null(); ++current_entry)
          // Skip entries that should not be serialized.
          if (current_entry->get_serialize()) {
            uint32_t rb;
            switch (current_entry->get_type()) {
              case mapping::source::BOOL:
                rb = set_boolean(*t, *current_entry, buffer, size);
                break;
              case mapping::source::DOUBLE:
                rb = set_double(*t, *current_entry, buffer, size);
                break;
              case mapping::source::INT:
                rb = set_integer(*t, *current_entry, buffer, size);
                break;
              case mapping::source::SHORT:
                rb = set_short(*t, *current_entry, buffer, size);
                break;
              case mapping::source::STRING:
                rb = set_string(*t, *current_entry, buffer, size);
                break;
              case mapping::source::TIME:
                rb = set_timestamp(*t, *current_entry, buffer, size);
                break;
              case mapping::source::UINT:
                rb = set_uint(*t, *current_entry, buffer, size);
                break;
              case mapping::source::ULONG:
                rb = set_ulong(*t, *current_entry, buffer, size);
                break;

              default:
                SPDLOG_LOGGER_ERROR(
                    _logger,
                    "BBDO: invalid mapping for object of type '{0}': {1} is "
                    "not a known type ID",
                    info->get_name(), current_entry->get_type());
                throw msg_fmt(
                    "BBDO: invalid mapping for object of type '{}"
                    "': {} is not a known type ID",
                    info->get_name(), current_entry->get_type());
            }
            buffer += rb;
            size -= rb;
          }
        return t.release();
      } else {
        SPDLOG_LOGGER_ERROR(
            _logger,
            "BBDO: cannot create object of ID {} whereas it has been "
            "registered",
            event_type);
        throw msg_fmt(
            "BBDO: cannot create object of ID {}"
            " whereas it has been registered",
            event_type);
      }
    } else {
      std::unique_ptr<io::data> t(
          info->get_operations().unserialize(buffer, size));
      if (t) {
        t->source_id = source_id;
        t->destination_id = destination_id;
      } else {
        SPDLOG_LOGGER_ERROR(
            _logger,
            "BBDO: cannot create object of ID {} whereas it has been "
            "registered",
            event_type);
        throw msg_fmt(
            "BBDO: cannot create object of ID {} whereas it has been "
            "registered",
            event_type);
      }
      return t.release();
    }
  } else {
    SPDLOG_LOGGER_INFO(
        _logger,
        "BBDO: cannot unserialize event of ID {}: event was not registered and "
        "will therefore be ignored",
        event_type);
  }

  return nullptr;
}

/**
 *  Get a boolean from an object.
 */
static void get_boolean(io::data const& t,
                        mapping::entry const& member,
                        std::vector<char>& buffer) {
  char c(member.get_bool(t) ? 1 : 0);
  buffer.push_back(c);
}

/**
 *  Get a double from an object.
 */
static void get_double(io::data const& t,
                       mapping::entry const& member,
                       std::vector<char>& buffer) {
  char str[32];
  size_t strsz(snprintf(str, sizeof(str), "%f", member.get_double(t)) + 1);
  if (strsz > sizeof(str))
    strsz = sizeof(str);
  std::copy(str, str + strsz, std::back_inserter(buffer));
}

/**
 *  Get an integer from an object.
 */
static void get_integer(io::data const& t,
                        mapping::entry const& member,
                        std::vector<char>& buffer) {
  uint32_t value(htonl(member.get_int(t)));
  char* v(reinterpret_cast<char*>(&value));
  std::copy(v, v + sizeof(value), std::back_inserter(buffer));
}

/**
 *  Get a short from an object.
 */
static void get_short(io::data const& t,
                      mapping::entry const& member,
                      std::vector<char>& buffer) {
  uint16_t value(htons(member.get_short(t)));
  char* v(reinterpret_cast<char*>(&value));
  std::copy(v, v + sizeof(value), std::back_inserter(buffer));
}

/**
 *  Get a string from an object.
 */
static void get_string(io::data const& t,
                       mapping::entry const& member,
                       std::vector<char>& buffer) {
  std::string const& tmp(member.get_string(t));
  std::copy(tmp.c_str(), tmp.c_str() + tmp.size() + 1,
            std::back_inserter(buffer));
}

/**
 *  Get a timestamp from an object.
 */
static void get_timestamp(io::data const& t,
                          mapping::entry const& member,
                          std::vector<char>& buffer) {
  uint64_t ts(member.get_time(t).get_time_t());
  uint32_t high{htonl(ts >> 32)};
  uint32_t low{htonl(ts & 0xffffffff)};
  char* vh{reinterpret_cast<char*>(&high)};
  char* vl{reinterpret_cast<char*>(&low)};
  std::copy(vh, vh + sizeof(high), std::back_inserter(buffer));
  std::copy(vl, vl + sizeof(low), std::back_inserter(buffer));
}

/**
 *  Get an uint32_teger from an object.
 */
static void get_uint(io::data const& t,
                     mapping::entry const& member,
                     std::vector<char>& buffer) {
  uint32_t value{htonl(member.get_uint(t))};
  char* v{reinterpret_cast<char*>(&value)};
  std::copy(v, v + sizeof(value), std::back_inserter(buffer));
}

/**
 *  Get an uint64_teger from an object.
 */
static void get_ulong(io::data const& t,
                      mapping::entry const& member,
                      std::vector<char>& buffer) {
  uint64_t value{member.get_ulong(t)};
  uint32_t high{htonl(value >> 32)};
  uint32_t low{htonl(value & 0xffffffff)};
  char* vh{reinterpret_cast<char*>(&high)};
  char* vl{reinterpret_cast<char*>(&low)};
  std::copy(vh, vh + sizeof(high), std::back_inserter(buffer));
  std::copy(vl, vl + sizeof(low), std::back_inserter(buffer));
}

/**
 *  Serialize an event in the BBDO protocol.
 *
 *  @param[in] e  Event to serialize.
 *
 *  @return Serialized event.
 */
io::raw* stream::serialize(const io::data& e) {
  std::deque<std::vector<char>> queue;

  // Get event info (mapping).
  const io::event_info* info = io::events::instance().get_event_info(e.type());
  if (info) {
    // Serialize properties of the object.
    const mapping::entry* current_entry = info->get_mapping();
    if (current_entry) {
      // Serialization buffer.
      queue.emplace_back(std::vector<char>());
      auto* header = &queue.back();
      header->resize(BBDO_HEADER_SIZE);
      queue.emplace_back(std::vector<char>());
      auto* content = &queue.back();

      for (mapping::entry const* current_entry(info->get_mapping());
           !current_entry->is_null(); ++current_entry) {
        // Skip entries that should not be serialized.
        if (current_entry->get_serialize())
          switch (current_entry->get_type()) {
            case mapping::source::BOOL:
              get_boolean(e, *current_entry, *content);
              break;
            case mapping::source::DOUBLE:
              get_double(e, *current_entry, *content);
              break;
            case mapping::source::INT:
              get_integer(e, *current_entry, *content);
              break;
            case mapping::source::SHORT:
              get_short(e, *current_entry, *content);
              break;
            case mapping::source::STRING:
              get_string(e, *current_entry, *content);
              break;
            case mapping::source::TIME:
              get_timestamp(e, *current_entry, *content);
              break;
            case mapping::source::UINT:
              get_uint(e, *current_entry, *content);
              break;
            case mapping::source::ULONG:
              get_ulong(e, *current_entry, *content);
              break;
            default:
              SPDLOG_LOGGER_ERROR(
                  _logger,
                  "BBDO: invalid mapping for object of type '{}': {} is not a "
                  "known type ID",
                  info->get_name(), current_entry->get_type());
              throw msg_fmt(
                  "BBDO: invalid mapping for object"
                  " of type '{}"
                  "': {}"
                  " is not a known type ID",
                  info->get_name(), current_entry->get_type());
          }

        // Packet splitting.
        while (content->size() >= 0xffff) {
          queue.emplace_back(std::vector<char>());
          auto* new_header = &queue.back();
          new_header->resize(BBDO_HEADER_SIZE);
          queue.emplace_back(content->begin() + 0xffff, content->end());
          content->resize(0xffff);
          auto* new_content = &queue.back();
          *(reinterpret_cast<uint16_t*>(header->data() + 2)) = 0xffff;
          *(reinterpret_cast<uint32_t*>(header->data() + 4)) = htonl(e.type());
          *(reinterpret_cast<uint32_t*>(header->data() + 8)) =
              htonl(e.source_id);
          *(reinterpret_cast<uint32_t*>(header->data() + 12)) =
              htonl(e.destination_id);

          *(reinterpret_cast<uint16_t*>(header->data())) = htons(
              misc::crc16_ccitt(header->data() + 2, BBDO_HEADER_SIZE - 2));
          content = new_content;
          header = new_header;
        }
      }

      *(reinterpret_cast<uint16_t*>(header->data() + 2)) =
          htons(content->size());
      *(reinterpret_cast<uint32_t*>(header->data() + 4)) = htonl(e.type());
      *(reinterpret_cast<uint32_t*>(header->data() + 8)) = htonl(e.source_id);
      *(reinterpret_cast<uint32_t*>(header->data() + 12)) =
          htonl(e.destination_id);

      *(reinterpret_cast<uint16_t*>(header->data())) =
          htons(misc::crc16_ccitt(header->data() + 2, BBDO_HEADER_SIZE - 2));

    } else {
      /* Here is the protobuf case: no mapping */
      std::string r{info->get_operations().serialize(e)};
      size_t size = r.size();
      auto it = r.begin();
      do {
        // Serialization buffer.
        queue.emplace_back(std::vector<char>());
        auto* content = &queue.back();
        content->resize(BBDO_HEADER_SIZE);
        if (size < 0xffff) {
          content->insert(content->end(), it, r.end());
          *(reinterpret_cast<uint16_t*>(content->data() + 2)) = htons(size);
          *(reinterpret_cast<uint32_t*>(content->data() + 4)) = htonl(e.type());
          *(reinterpret_cast<uint32_t*>(content->data() + 8)) =
              htonl(e.source_id);
          *(reinterpret_cast<uint32_t*>(content->data() + 12)) =
              htonl(e.destination_id);

          *(reinterpret_cast<uint16_t*>(content->data())) = htons(
              misc::crc16_ccitt(content->data() + 2, BBDO_HEADER_SIZE - 2));
          break;
        } else {
          content->insert(content->end(), it, it + 0xffff);
          *(reinterpret_cast<uint16_t*>(content->data() + 2)) = 0xffff;
          *(reinterpret_cast<uint32_t*>(content->data() + 4)) = htonl(e.type());
          *(reinterpret_cast<uint32_t*>(content->data() + 8)) =
              htonl(e.source_id);
          *(reinterpret_cast<uint32_t*>(content->data() + 12)) =
              htonl(e.destination_id);

          *(reinterpret_cast<uint16_t*>(content->data())) = htons(
              misc::crc16_ccitt(content->data() + 2, BBDO_HEADER_SIZE - 2));
          size -= 0xffff;
          it += 0xffff;
        }
      } while (size > 0);
    }

    // Finalization: concatenation of all the vectors in the queue.
    size_t size = 0;
    for (auto& v : queue)
      size += v.size();

    // Serialization buffer.
    std::unique_ptr<io::raw> buffer(std::make_unique<io::raw>());
    std::vector<char>& data(buffer->get_buffer());
    data.reserve(size);
    for (auto& v : queue)
      data.insert(data.end(), v.begin(), v.end());

    return buffer.release();
  } else {
    SPDLOG_LOGGER_INFO(
        _logger,
        "BBDO: cannot serialize event of ID {}: event was not registered and "
        "will therefore be ignored",
        e.type());
  }

  return nullptr;
}

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
    : io::stream("BBDO"),
      _skipped(0),
      _is_input{is_input},
      _coarse(false),
      _negotiate{true},
      _negotiated{false},
      _timeout(5),
      _acknowledged_events{0},
      _ack_limit(1000),
      _events_received_since_last_ack(0),
      _last_sent_ack(time(nullptr)),
      _grpc_serialized(grpc_serialized),
      _extensions{extensions},
      _bbdo_version(config::applier::state::instance().get_bbdo_version()),
      _logger{log_v2::instance().get(log_v2::BBDO)} {
  SPDLOG_LOGGER_DEBUG(log_v2::instance().get(log_v2::CORE),
                      "create bbdo stream {:p}",
                      static_cast<const void*>(this));
}

/**
 * @brief Destroy the stream::stream object
 *
 */
stream::~stream() {
  SPDLOG_LOGGER_DEBUG(_logger, "destroy bbdo stream {}",
                      static_cast<void*>(this));
}

/**
 * @brief All the mecanism behind this stream is stopped once this method is
 * called. The last thing done is to return how many events are acknowledged.
 *
 * @return The number of events to acknowledge.
 */
int32_t stream::stop() {
  _logger->debug("bbdo::stream stop {}", static_cast<void*>(this));
  /* A concrete explanation:
   * I'm engine and my work is to send data to broker.
   * Here, the user wants to stop me/ I need to ask broker how many
   * data I can acknowledge. */
  if (!_is_input) {
    try {
      _send_event_stop_and_wait_for_ack();
    } catch (const std::exception& e) {
      _logger->info(
          "BBDO: unable to send stop message to peer, it is already "
          "stopped: "
          "{}",
          e.what());
    }
  }

  /* We acknowledge peer about received events. */
  _logger->info("bbdo stream stopped with {} events acknowledged",
                _events_received_since_last_ack);
  if (_events_received_since_last_ack)
    send_event_acknowledgement();

  _substream->stop();

  /* We return the number of events handled by our stream. */
  int32_t retval = _acknowledged_events;
  _acknowledged_events = 0;
  if (_poller_id && !_broker_name.empty() && !_poller_name.empty())
    config::applier::state::instance().remove_peer(_poller_id, _poller_name,
                                                   _broker_name);
  return retval;
}

/**
 *  Flush stream data.
 *
 *  @return Number of acknowledged events.
 */
int stream::flush() {
  _substream->flush();
  int retval = _acknowledged_events;
  _acknowledged_events -= retval;
  return retval;
}

/**
 * @brief This method is called from a feeder stream. It is called when the
 * stream is goint soon to be stopped. It sends a stop message to the peer to
 * count how many events can be acknowledged.
 */
void stream::_send_event_stop_and_wait_for_ack() {
  if (!_coarse) {
    SPDLOG_LOGGER_DEBUG(_logger, "BBDO: sending stop packet to peer");
    /* Here, we send a bbdo::pb_stop that passes through the network contrary
     * to the local::pb_stop. */
    std::shared_ptr<bbdo::pb_stop> stop_packet{
        std::make_shared<bbdo::pb_stop>()};
    stop_packet->mut_obj().set_poller_id(
        config::applier::state::instance().poller_id());
    _write(stop_packet);

    SPDLOG_LOGGER_DEBUG(_logger, "BBDO: retrieving ack packet from peer");
    std::shared_ptr<io::data> d;
    time_t deadline = time(nullptr) + 5;

    _read_any(d, deadline);
    if (!d) {
      SPDLOG_LOGGER_ERROR(
          _logger,
          "BBDO: no message received from peer. Cannot acknowledge properly "
          "waiting messages before stopping.");
      return;
    }
    switch (d->type()) {
      case ack::static_type():
        SPDLOG_LOGGER_INFO(
            _logger,
            "BBDO: received acknowledgement for {} events before finishing",
            std::static_pointer_cast<ack const>(d)->acknowledged_events);
        acknowledge_events(
            std::static_pointer_cast<ack const>(d)->acknowledged_events);
        break;
      case pb_ack::static_type():
        SPDLOG_LOGGER_INFO(
            _logger,
            "BBDO: received acknowledgement for {} events before finishing",
            std::static_pointer_cast<const pb_ack>(d)
                ->obj()
                .acknowledged_events());
        acknowledge_events(std::static_pointer_cast<const pb_ack>(d)
                               ->obj()
                               .acknowledged_events());
        break;
      default:
        SPDLOG_LOGGER_ERROR(
            _logger,
            "BBDO: wrong message received (type {}) - expected ack event",
            d->type());
        break;
    }
  }
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

  // Send our own packet if we should be first.
  if (neg == negotiate_first) {
    SPDLOG_LOGGER_DEBUG(
        _logger, "BBDO: sending welcome packet (available extensions: {})",
        extensions);
    /* if _negotiate, we send all the extensions we would like to have,
     * otherwise we only send the mandatory extensions */
    if (_bbdo_version.total_version <= v300.total_version) {
      auto welcome_packet{
          std::make_shared<version_response>(_bbdo_version, extensions)};
      _write(welcome_packet);
    } else {
      auto welcome{std::make_shared<pb_welcome>()};
      auto& obj = welcome->mut_obj();
      obj.mutable_version()->set_major(_bbdo_version.major_v);
      obj.mutable_version()->set_minor(_bbdo_version.minor_v);
      obj.mutable_version()->set_patch(_bbdo_version.patch);
      obj.set_extensions(extensions);
      obj.set_poller_id(config::applier::state::instance().poller_id());
      obj.set_poller_name(config::applier::state::instance().poller_name());
      obj.set_broker_name(config::applier::state::instance().broker_name());
      obj.set_peer_type(config::applier::state::instance().peer_type());
      /* I know I'm Engine, and I have access to the configuration. */
      if (!config::applier::state::instance().engine_config_dir().empty())
        obj.set_extended_negotiation(true);
      /* I know I'm Broker, and I have access to the php cache configuration
       * directory. */
      else if (!config::applier::state::instance().config_cache_dir().empty())
        obj.set_extended_negotiation(true);
      /* I don't know what I am. */
      else
        obj.set_extended_negotiation(false);
      _write(welcome);
    }
  }

  // Read peer packet.
  SPDLOG_LOGGER_DEBUG(_logger, "BBDO: retrieving welcome packet of peer");
  std::shared_ptr<io::data> d;
  time_t deadline;
  if (_timeout == (time_t)-1)
    deadline = (time_t)-1;
  else
    deadline = time(nullptr) + _timeout;

  _read_any(d, deadline);
  if (!d || (d->type() != version_response::static_type() &&
             d->type() != pb_welcome::static_type())) {
    std::string msg;
    if (d)
      msg = fmt::format(
          "BBDO: invalid protocol header, aborting connection: waiting for "
          "message of type '{}' but received type is {}",
          _bbdo_version.total_version > v300.total_version ? "pb_welcome"
                                                           : "version_response",
          d->type());
    else
      msg = fmt::format(
          "BBDO: invalid protocol header, aborting connection: waiting for "
          "message of type '{}' but nothing received",
          _bbdo_version.total_version > v300.total_version
              ? "pb_welcome"
              : "version_response");
    SPDLOG_LOGGER_ERROR(_logger, msg);
    throw msg_fmt(msg);
  }

  std::string peer_extensions;
  _extended_negotiation = false;

  if (d->type() == version_response::static_type()) {
    std::shared_ptr<version_response> v(
        std::static_pointer_cast<version_response>(d));
    if (v->bbdo_major != _bbdo_version.major_v) {
      SPDLOG_LOGGER_ERROR(
          _logger,
          "BBDO: peer is using protocol version {}.{}.{} whereas we're using "
          "protocol version {}.{}.{}",
          v->bbdo_major, v->bbdo_minor, v->bbdo_patch, _bbdo_version.major_v,
          _bbdo_version.minor_v, _bbdo_version.patch);
      throw msg_fmt(
          "BBDO: peer is using protocol version {}.{}.{}"
          " whereas we're using protocol version {}.{}.{}",
          v->bbdo_major, v->bbdo_minor, v->bbdo_patch, _bbdo_version.major_v,
          _bbdo_version.minor_v, _bbdo_version.patch);
    }
    SPDLOG_LOGGER_INFO(
        _logger,
        "BBDO: peer is using protocol version {}.{}.{}, we're using version "
        "{}.{}.{}",
        v->bbdo_major, v->bbdo_minor, v->bbdo_patch, _bbdo_version.major_v,
        _bbdo_version.minor_v, _bbdo_version.patch);

    // Send our own packet if we should be second.
    if (neg == negotiate_second) {
      SPDLOG_LOGGER_DEBUG(
          _logger, "BBDO: sending welcome packet (available extensions: {})",
          extensions);
      /* if _negotiate, we send all the extensions we would like to have,
       * otherwise we only send the mandatory extensions */
      auto welcome_packet(
          std::make_shared<version_response>(_bbdo_version, extensions));
      _write(welcome_packet);
      _substream->flush();
    }
    peer_extensions = v->extensions;
  } else {
    std::shared_ptr<pb_welcome> w(std::static_pointer_cast<pb_welcome>(d));
    const auto& pb_version = w->obj().version();
    if (pb_version.major() != _bbdo_version.major_v) {
      SPDLOG_LOGGER_ERROR(
          _logger,
          "BBDO: peer is using protocol version {}.{}.{} whereas we're using "
          "protocol version {}.{}.{}",
          pb_version.major(), pb_version.minor(), pb_version.patch(),
          _bbdo_version.major_v, _bbdo_version.minor_v, _bbdo_version.patch);
      throw msg_fmt(
          "BBDO: peer is using protocol version {}.{}.{}"
          " whereas we're using protocol version {}.{}.{}",
          pb_version.major(), pb_version.minor(), pb_version.patch(),
          _bbdo_version.major_v, _bbdo_version.minor_v, _bbdo_version.patch);
    }
    SPDLOG_LOGGER_INFO(
        _logger,
        "BBDO: peer is using protocol version {}.{}.{}, we're using version "
        "{}.{}.{}",
        pb_version.major(), pb_version.minor(), pb_version.patch(),
        _bbdo_version.major_v, _bbdo_version.minor_v, _bbdo_version.patch);

    // Send our own packet if we should be second.
    if (neg == negotiate_second) {
      SPDLOG_LOGGER_DEBUG(
          _logger, "BBDO: sending welcome packet (available extensions: {})",
          extensions);
      /* if _negotiate, we send all the extensions we would like to have,
       * otherwise we only send the mandatory extensions */
      auto welcome(std::make_shared<pb_welcome>());
      auto& obj = welcome->mut_obj();
      obj.mutable_version()->set_major(_bbdo_version.major_v);
      obj.mutable_version()->set_minor(_bbdo_version.minor_v);
      obj.mutable_version()->set_patch(_bbdo_version.patch);
      obj.set_extensions(extensions);
      obj.set_poller_id(config::applier::state::instance().poller_id());
      obj.set_poller_name(config::applier::state::instance().poller_name());
      obj.set_broker_name(config::applier::state::instance().broker_name());
      obj.set_peer_type(config::applier::state::instance().peer_type());
      /* I know I'm Engine, and I have access to the configuration directory. */
      if (!config::applier::state::instance().engine_config_dir().empty())
        obj.set_extended_negotiation(true);
      /* I know I'm Broker, and I have access to the php cache configuration
       * directory. */
      else if (!config::applier::state::instance().config_cache_dir().empty())
        obj.set_extended_negotiation(true);
      /* I don't have access to any configuration directory. */
      else
        obj.set_extended_negotiation(false);

      _write(welcome);
      _substream->flush();
    }
    peer_extensions = w->obj().extensions();
    _poller_id = w->obj().poller_id();
    _poller_name = w->obj().poller_name();
    _broker_name = w->obj().broker_name();

    _peer_type = w->obj().peer_type();
    if (_peer_type != common::UNKNOWN) {
      /* We are in the bbdo stream, _poller_id, _broker_name,
       * _extended_negotiation are informations about the peer, not us. */
      _extended_negotiation = true;
    }
  }

  // Negotiation.
  std::list<std::string> running_config = get_running_config();

  // Apply negotiated extensions.
  SPDLOG_LOGGER_INFO(
      _logger, "BBDO: we have extensions '{}' and peer has '{}'", extensions,
      fmt::string_view(peer_extensions.data(), peer_extensions.size()));
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
        for (std::map<std::string, io::protocols::protocol>::const_iterator
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
  if (_poller_id > 0 && !_broker_name.empty()) {
    config::applier::state::instance().add_peer(_poller_id, _poller_name,
                                                _broker_name, _peer_type,
                                                _extended_negotiation);
  }
  SPDLOG_LOGGER_TRACE(_logger, "Negotiation done.");
}

std::list<std::string> stream::get_running_config() {
  std::list<std::string> retval;
  std::shared_ptr<io::stream> substream = get_substream();
  while (substream) {
    retval.push_back(substream->get_name());
    substream = substream->get_substream();
  }
  return retval;
}

/**
 * @brief Handle a BBDO event. Events of category io::bbdo are the guardians of
 * BBDO messages. These messages are used by the protocol itself and are always
 * prioritized.
 *
 * @param d The event to handle.
 */
void stream::_handle_bbdo_event(const std::shared_ptr<io::data>& d) {
  switch (d->type()) {
    case version_response::static_type(): {
      auto version(std::static_pointer_cast<version_response>(d));
      if (version->bbdo_major != _bbdo_version.major_v) {
        SPDLOG_LOGGER_ERROR(
            _logger,
            "BBDO: peer is using protocol version {}.{}.{}, whereas we're "
            "using protocol version {}.{}.{}",
            version->bbdo_major, version->bbdo_minor, version->bbdo_patch,
            _bbdo_version.major_v, _bbdo_version.minor_v, _bbdo_version.patch);
        throw msg_fmt(
            "BBDO: peer is using protocol version {}.{}.{} "
            "whereas we're using protocol version {}.{}.{}",
            version->bbdo_major, version->bbdo_minor, version->bbdo_patch,
            _bbdo_version.major_v, _bbdo_version.minor_v, _bbdo_version.patch);
      }
      SPDLOG_LOGGER_INFO(
          _logger,
          "BBDO: peer is using protocol version {}.{}.{} , we're using "
          "version "
          "{}.{}.{}",
          version->bbdo_major, version->bbdo_minor, version->bbdo_patch,
          _bbdo_version.major_v, _bbdo_version.minor_v, _bbdo_version.patch);

      break;
    }
    case pb_welcome::static_type(): {
      auto welcome(std::static_pointer_cast<pb_welcome>(d));
      const auto& pb_version = welcome->obj().version();
      if (pb_version.major() != _bbdo_version.major_v) {
        SPDLOG_LOGGER_ERROR(
            _logger,
            "BBDO: peer is using protocol version {}.{}.{}, whereas we're "
            "using protocol version {}.{}.{}",
            pb_version.major(), pb_version.minor(), pb_version.patch(),
            _bbdo_version.major_v, _bbdo_version.minor_v, _bbdo_version.patch);
        throw msg_fmt(
            "BBDO: peer is using protocol version {}.{}.{} "
            "whereas we're using protocol version {}.{}.{}",
            pb_version.major(), pb_version.minor(), pb_version.patch(),
            _bbdo_version.major_v, _bbdo_version.minor_v, _bbdo_version.patch);
      }
      SPDLOG_LOGGER_INFO(
          _logger,
          "BBDO: peer is using protocol version {}.{}.{} , we're using "
          "version "
          "{}.{}.{}",
          pb_version.major(), pb_version.minor(), pb_version.patch(),
          _bbdo_version.major_v, _bbdo_version.minor_v, _bbdo_version.patch);
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
    case pb_engine_configuration::static_type(): {
      const EngineConfiguration& ec =
          std::static_pointer_cast<pb_engine_configuration>(d)->obj();
      /* Here, we are Broker and peer is Engine. */
      if (config::applier::state::instance().peer_type() == common::BROKER &&
          _peer_type == common::ENGINE) {
        SPDLOG_LOGGER_INFO(
            _logger,
            "BBDO: received engine configuration from Engine peer '{}'",
            ec.broker_name());
        bool match = check_poller_configuration(ec.poller_id(),
                                                ec.engine_config_version());
        auto engine_conf = std::make_shared<pb_engine_configuration>();
        auto& obj = engine_conf->mut_obj();
        obj.set_poller_id(config::applier::state::instance().poller_id());
        obj.set_poller_name(config::applier::state::instance().poller_name());
        obj.set_broker_name(config::applier::state::instance().broker_name());
        obj.set_peer_type(common::BROKER);
        if (match) {
          SPDLOG_LOGGER_INFO(
              _logger, "BBDO: engine configuration for '{}' is up to date",
              ec.broker_name());
          obj.set_need_update(false);
        } else {
          SPDLOG_LOGGER_INFO(_logger,
                             "BBDO: engine configuration for '{}' is "
                             "outdated",
                             ec.broker_name());
          /* engine_conf has a new version, it is sent to engine. And engine
           * will send its configuration to broker. */
          obj.set_need_update(true);
        }
        _write(engine_conf);
      }
    } break;
    default:
      break;
  }
}

/**
 * @brief Wait for a BBDO event (category io::bbdo) of a specific type. While
 * received events are of category io::bbdo, they are handled as usual, and when
 * the expected event is received, it is returned. The expected event is not
 * handled.
 *
 * @param expected_type The expected type of the event.
 * @param d The event that was received with the expected type.
 * @param deadline The deadline in seconds.
 *
 * @return true if the expected event was received before the deadline, false
 * otherwise.
 */
bool stream::_wait_for_bbdo_event(uint32_t expected_type,
                                  std::shared_ptr<io::data>& d,
                                  time_t deadline) {
  for (;;) {
    bool timed_out = !_read_any(d, deadline);
    uint32_t event_id = !d ? 0 : d->type();
    if (timed_out || (event_id >> 16) != io::bbdo)
      return false;

    if (event_id == expected_type)
      return true;

    _handle_bbdo_event(d);

    // Control messages.
    SPDLOG_LOGGER_DEBUG(
        _logger,
        "BBDO: event with ID {} was a control message, launching recursive "
        "read",
        event_id);
  }

  return false;
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
bool stream::read(std::shared_ptr<io::data>& d, time_t deadline) {
  // Read event.
  d.reset();

  bool timed_out = !_read_any(d, deadline);
  uint32_t event_id = !d ? 0 : d->type();

  while (!timed_out && (event_id >> 16) == io::bbdo) {
    _handle_bbdo_event(d);

    // Control messages.
    SPDLOG_LOGGER_DEBUG(
        _logger,
        "BBDO: event with ID {} was a control message, launching recursive "
        "read",
        event_id);
    timed_out = !_read_any(d, deadline);
    event_id = !d ? 0 : d->type();
  }

  /* If !timed_out, then we have two possibilities:
   *  * we get an event d
   *  * an event has been returned but we could not unserialize it.
   */
  if (!timed_out) {
    ++_events_received_since_last_ack;
    SPDLOG_LOGGER_TRACE(_logger, "{} events to acknowledge",
                        _events_received_since_last_ack);
  }
  time_t now = time(nullptr);
  if (_events_received_since_last_ack >= _ack_limit ||
      (_events_received_since_last_ack && _last_sent_ack + 5 < now)) {
    _last_sent_ack = now;
    send_event_acknowledgement();
  }
  return !timed_out;
}

/**
 *  @brief Get the next available event.
 *
 *  Extract the next available event on the input stream, NULL if the
 *  stream is closed.
 *
 *  @param[out] d         Next available event.
 *  @param[in]  deadline  Timeout.
 *
 *  @return Respect io::stream::read()'s return value.
 */
bool stream::_read_any(std::shared_ptr<io::data>& d, time_t deadline) {
  try {
    // Return value.
    std::unique_ptr<io::data> e;
    d.reset();

    for (;;) {
      /* Maybe we have to complete the header. */
      _read_packet(BBDO_HEADER_SIZE, deadline);
      if (!_grpc_serialized_queue.empty()) {
        d = _grpc_serialized_queue.front();
        SPDLOG_LOGGER_TRACE(_logger, "read event: {}", *d);
        _grpc_serialized_queue.pop_front();
        return true;
      }

      // Packet size is now at least BBDO_HEADER_SIZE and maybe contains
      // already a full BBDO packet.

      const char* pack = _packet.data();
      uint16_t chksum = ntohs(*reinterpret_cast<uint16_t const*>(pack));
      uint32_t packet_size =
          ntohs(*reinterpret_cast<uint16_t const*>(pack + 2));
      uint32_t event_id = ntohl(*reinterpret_cast<uint32_t const*>(pack + 4));
      uint32_t source_id = ntohl(*reinterpret_cast<uint32_t const*>(pack + 8));
      uint32_t dest_id = ntohl(*reinterpret_cast<uint32_t const*>(pack + 12));
      uint16_t expected = misc::crc16_ccitt(pack + 2, BBDO_HEADER_SIZE - 2);

      SPDLOG_LOGGER_TRACE(_logger,
                          "Reading: header eventID {} sourceID {} destID {} "
                          "checksum {:x} and "
                          "expected {:x}",
                          event_id, source_id, dest_id, chksum, expected);

      if (expected != chksum) {
        // The packet is corrupted.
        if (_skipped == 0) {
          // First corrupted byte.
          SPDLOG_LOGGER_ERROR(
              _logger,
              "peer {} is sending corrupted data: invalid CRC: {:04x} != "
              "{:04x}",
              peer(), chksum, expected);
        }
        ++_skipped;
        _packet.erase(_packet.begin());
        continue;
      } else if (_skipped) {
        SPDLOG_LOGGER_INFO(
            _logger,
            "peer {} sent {} corrupted payload bytes, resuming processing",
            peer(), _skipped);
        _skipped = 0;
      }

      // It is time to finish to read the packet.

      _read_packet(BBDO_HEADER_SIZE + packet_size, deadline);

      // Now, _packet contains at least BBDO_HEADER_SIZE + packet_size bytes.

      std::vector<char> content;
      if (_packet.size() == BBDO_HEADER_SIZE + packet_size) {
        SPDLOG_LOGGER_TRACE(
            _logger, "packet matches header + content => extracting content");
        // We remove the header from the packet: FIXME DBR this is not
        // beautiful...

        content = std::vector<char>(_packet.begin() + BBDO_HEADER_SIZE,
                                    _packet.end());
        _packet.clear();
        // The size should be of only packet_size now.
      } else {
        /* we have _packet.size() > BBDO_HEADER_SIZE + packet_size */

        size_t previous_packet_size = _packet.size();
        // packet contains more than one BBDO packet...
        content =
            std::vector<char>(_packet.begin() + BBDO_HEADER_SIZE,
                              _packet.begin() + BBDO_HEADER_SIZE + packet_size);
        _packet.erase(_packet.begin(),
                      _packet.begin() + BBDO_HEADER_SIZE + packet_size);
        SPDLOG_LOGGER_TRACE(
            _logger,
            "packet longer than header + content => splitting the whole of "
            "size {} to content of size {} and remaining of size {}",
            previous_packet_size, content.size(), _packet.size());
      }

      if (packet_size != 0xffff) {
        // Cool we can work with it!

        // Is it the next part of an already known input buffer?
        for (auto it = _buffer.begin(); it != _buffer.end(); ++it) {
          auto& b = *it;
          if (b.matches(event_id, source_id, dest_id)) {
            // Good, we've found it.
            b.push_back(std::move(content));
            content = b.to_vector();
            _buffer.erase(it);
            break;
          }
        }
        /* There is no reason to have this but no one knows. */
        if (_buffer.size() > 0) {
          SPDLOG_LOGGER_ERROR(_logger,
                              "There are still {} long BBDO packets that "
                              "cannot be sent, this "
                              "maybe be due to a corrupted retention file.",
                              _buffer.size());
          /* In case of too many long events stored in memory, we purge the
           * oldest ones. */
          while (_buffer.size() > 3) {
            SPDLOG_LOGGER_INFO(
                _logger,
                "One too old long event part of type {} removed from memory",
                _buffer.front().get_event_id());
            _buffer.pop_front();
          }
        }

        pack = content.data();

        // Maybe it is bigger now.
        packet_size = content.size();
        d.reset(unserialize(event_id, source_id, dest_id, pack, packet_size));
        if (d) {
          SPDLOG_LOGGER_TRACE(_logger,
                              "unserialized {} bytes for event of type {}",
                              BBDO_HEADER_SIZE + packet_size, event_id);
        } else {
          SPDLOG_LOGGER_WARN(_logger,
                             "unknown event type {} event cannot be decoded",
                             event_id);
          SPDLOG_LOGGER_TRACE(_logger, "discarded {} bytes",
                              BBDO_HEADER_SIZE + packet_size);
        }
        return true;
      } else {
        // Is it the next part of an already known input buffer?
        bool done = false;
        for (auto it = _buffer.begin(); it != _buffer.end(); ++it) {
          auto& b = *it;
          if (b.matches(event_id, source_id, dest_id)) {
            // Good, we've found it.
            b.push_back(std::move(content));
            content.clear();
            done = true;
            break;
          }
        }
        if (!done)
          _buffer.emplace_back(
              buffer(event_id, source_id, dest_id, std::move(content)));

        /* There is no reason to have this but no one knows. */
        if (_buffer.size() > 1) {
          SPDLOG_LOGGER_ERROR(
              _logger,
              "There are {} long BBDO packets waiting for their missing "
              "parts "
              "in memory, this may be due to a corrupted retention file.",
              _buffer.size());
          /* In case of too many long events stored in memory, we purge the
           * oldest ones. */
          while (_buffer.size() > 4) {
            SPDLOG_LOGGER_INFO(
                _logger,
                "One too old long event part of type {} removed from memory",
                _buffer.front().get_event_id());
            _buffer.pop_front();
          }
        }
      }
    }
  } catch (const exceptions::timeout& e) {
    return false;
  }
  return false;
}

/**
 * @brief Fill the internal _packet vector until it reaches the given size. It
 * may be bigger. The deadline is the limit time after that an exception is
 * thrown. Even if an exception is thrown the vector may begin to be fill, it
 * is just not finished, and so no data are lost. Received packets are BBDO
 * packets or maybe pieces of BBDO packets, so we keep vectors as is because
 * usually a vector should just represent a packet. In case of event
 * serialized only by grpc stream, we store it in _grpc_serialized_queue
 *
 * @param size The wanted final size
 * @param deadline A time_t.
 */
void stream::_read_packet(size_t size, time_t deadline) {
  // Read as much data as requested.
  while (_packet.size() < size) {
    std::shared_ptr<io::data> d;
    bool timeout = !_substream->read(d, deadline);

    if (d) {
      if (d->type() == io::raw::static_type()) {
        std::vector<char>& new_v =
            std::static_pointer_cast<io::raw>(d)->_buffer;
        if (!new_v.empty()) {
          if (_packet.size() == 0) {
            _packet = std::move(new_v);
            new_v.clear();
          } else
            _packet.insert(_packet.end(), new_v.begin(), new_v.end());
        }
      } else {
        _grpc_serialized_queue.push_back(d);
        return;
      }
    }
    if (timeout) {
      SPDLOG_LOGGER_TRACE(_logger,
                          "_read_packet timeout!!, size = {}, deadline = {}",
                          size, deadline);
      throw exceptions::timeout();
    }
  }
}

/**
 *  Set the limit of events received before an ack should be sent.
 *
 *  @param limit  The limit of events received before an ack should be sent.
 */
void stream::set_ack_limit(uint32_t limit) {
  _ack_limit = limit;
}

/**
 *  Set whether this stream is coarse or not.
 *
 *  @param[in] coarse  True if coarse.
 */
void stream::set_coarse(bool coarse) {
  _coarse = coarse;
}

/**
 *  Set whether or not the stream should negotiate features.
 *
 *  @param[in] negotiate   True if the stream should negotiate features.
 *  @param[in] extensions  Extensions supported by this stream.
 */
void stream::set_negotiate(bool negotiate) {
  _negotiate = negotiate;
}

/**
 *  Set the timeout supported by this stream.
 *
 *  @param[in] timeout  Timeout in seconds.
 */
void stream::set_timeout(int timeout) {
  _timeout = timeout;
}

/**
 *  Get statistics.
 *
 *  @param[out] tree Output tree.
 */
void stream::statistics(nlohmann::json& tree) const {
  tree["bbdo_input_ack_limit"] = static_cast<double>(_ack_limit);
  tree["bbdo_unacknowledged_events"] =
      static_cast<double>(_events_received_since_last_ack);

  if (_substream)
    _substream->statistics(tree);
}

void stream::_negotiate_engine_conf() {
  SPDLOG_LOGGER_DEBUG(_logger,
                      "BBDO: instance event sent to {} - supports "
                      "extended negotiation: {}",
                      _broker_name, _extended_negotiation);
  /* We are an Engine since we emit an instance event and we have an
   * engine config directory. If the Broker supports extended negotiation,
   * we send also an engine configuration event. And then we'll wait for
   * an answer from Broker. */
  if (_extended_negotiation &&
      !config::applier::state::instance().engine_config_dir().empty()) {
    auto engine_conf = std::make_shared<bbdo::pb_engine_configuration>();
    auto& obj = engine_conf->mut_obj();
    obj.set_poller_id(config::applier::state::instance().poller_id());
    obj.set_poller_name(config::applier::state::instance().poller_name());
    obj.set_broker_name(config::applier::state::instance().broker_name());
    obj.set_peer_type(common::ENGINE);

    /* Time to fill the config version. */
    std::error_code ec;
    _config_version = common::hash_directory(
        config::applier::state::instance().engine_config_dir(), ec);
    if (ec) {
      _logger->error(
          "BBDO: cannot access directory '{}': {}",
          config::applier::state::instance().engine_config_dir().string(),
          ec.message());
    }
    obj.set_engine_config_version(_config_version);
    _logger->info(
        "BBDO: engine configuration sent to peer '{}' with version {}",
        _broker_name, _config_version);
    _write(engine_conf);
    std::shared_ptr<io::data> d;
    time_t deadline = time(nullptr) + 5;
    bool found = _wait_for_bbdo_event(pb_engine_configuration::static_type(), d,
                                      deadline);
    if (!found) {
      _logger->warn(
          "BBDO: no engine configuration received from peer '{}' as "
          "response",
          _broker_name);
      if (d) {
        _logger->info(
            "BBDO: received message of type {:x} instead of "
            "pb_engine_configuration",
            d->type());
      } else
        _logger->info("BBDO: no message received");
    } else {
      _logger->debug(
          "BBDO: engine configuration from peer '{}' received as expected",
          _broker_name);
      const EngineConfiguration& ec =
          std::static_pointer_cast<pb_engine_configuration>(d)->obj();

      if (!ec.need_update()) {
        SPDLOG_LOGGER_INFO(_logger,
                           "BBDO: No engine configuration update needed");
        config::applier::state::instance().set_broker_needs_update(
            ec.poller_id(), ec.poller_name(), ec.broker_name(), common::BROKER,
            false);
      } else {
        SPDLOG_LOGGER_INFO(_logger,
                           "BBDO: Engine configuration needs to be updated");
        config::applier::state::instance().set_broker_needs_update(
            ec.poller_id(), ec.poller_name(), ec.broker_name(), common::BROKER,
            true);
      }
    }
  } else {
    /* Legacy negociation */
    config::applier::state::instance().set_peers_ready();
  }
}

void stream::_write(const std::shared_ptr<io::data>& d) {
  assert(d);

  if (d->type() == neb::pb_instance::static_type())
    _negotiate_engine_conf();

  if (!_grpc_serialized || !std::dynamic_pointer_cast<io::protobuf_base>(d)) {
    std::shared_ptr<io::raw> serialized(serialize(*d));
    if (serialized) {
      SPDLOG_LOGGER_TRACE(_logger,
                          "BBDO: serialized event of type {:x} to {} bytes",
                          d->type(), serialized->size());
      _substream->write(serialized);
    } else {
      SPDLOG_LOGGER_ERROR(_logger, "BBDO: cannot serialize event of type {:x}",
                          d->type());
    }
  } else
    _substream->write(d);
}

/**
 *  Write data to stream.
 *
 *  @param[in] d Data to send.
 *
 *  @return Number of events acknowledged.
 */
int32_t stream::write(std::shared_ptr<io::data> const& d) {
  _write(d);

  int32_t retval = _acknowledged_events;
  _acknowledged_events -= retval;
  return retval;
}

/**
 *  Acknowledge a certain amount of events.
 *
 *  @param[in] events  The amount of event.
 */
void stream::acknowledge_events(uint32_t events) {
  _acknowledged_events += events;
}

/**
 *  Send an acknowledgement for all the events received.
 */
void stream::send_event_acknowledgement() {
  if (!_coarse) {
    SPDLOG_LOGGER_DEBUG(_logger, "send acknowledgement for {} events",
                        _events_received_since_last_ack);
    if (_bbdo_version.total_version >= 0x0300000001) {
      std::shared_ptr<pb_ack> acknowledgement(std::make_shared<pb_ack>());
      acknowledgement->mut_obj().set_acknowledged_events(
          _events_received_since_last_ack);
      _write(acknowledgement);
    } else {
      std::shared_ptr<ack> acknowledgement(
          std::make_shared<ack>(_events_received_since_last_ack));
      _write(acknowledgement);
    }
    _events_received_since_last_ack = 0;
  }
}

/**
 * @brief Check if the poller configuration is up to date.
 *
 * @param poller_id
 * @param expected_version
 *
 * @return
 */
bool stream::check_poller_configuration(uint64_t poller_id,
                                        const std::string& expected_version) {
  std::error_code ec;
  const std::filesystem::path& pollers_conf_dir =
      config::applier::state::instance().pollers_config_dir();
  if (!std::filesystem::is_directory(pollers_conf_dir, ec)) {
    if (ec)
      _logger->error("Cannot access directory '{}': {}",
                     pollers_conf_dir.string(), ec.message());
    std::filesystem::create_directories(pollers_conf_dir, ec);
    if (ec) {
      _logger->error("Cannot create directory '{}': {}",
                     pollers_conf_dir.string(), ec.message());
      return false;
    }
  }
  auto poller_dir = pollers_conf_dir / fmt::to_string(poller_id);
  if (!std::filesystem::is_directory(poller_dir, ec)) {
    if (ec)
      _logger->error("Cannot access directory '{}': {}", poller_dir.string(),
                     ec.message());
    std::filesystem::create_directories(poller_dir, ec);
    if (ec)
      _logger->error("Cannot create directory '{}': {}", poller_dir.string(),
                     ec.message());
    return false;
  }
  std::string current = common::hash_directory(poller_dir, ec);
  if (ec) {
    _logger->error("Cannot access directory '{}': {}", poller_dir.string(),
                   ec.message());
    return false;
  }
  config::applier::state::instance().set_engine_configuration(poller_id,
                                                              current);
  return current == expected_version;
}
