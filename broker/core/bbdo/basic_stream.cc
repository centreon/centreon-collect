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

#include "broker/core/bbdo/basic_stream.hh"

#include <arpa/inet.h>

#include "bbdo/bbdo/ack.hh"
#include "bbdo/bbdo/stop.hh"
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
        "cannot extract double value: not terminating '\\0' in remaining {} "
        "bytes of packet",
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
    throw msg_fmt("BBDO: cannot extract integer value: {} bytes left in packet",
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
    throw msg_fmt("BBDO: cannot extract short value: {} bytes left in packet",
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
        "BBDO: cannot extract string value: no terminating '\\0' in remaining "
        "{} bytes of packet",
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
std::shared_ptr<io::data> basic_stream::deserialize(uint32_t event_type,
                                    uint32_t source_id,
                                    uint32_t destination_id,
                                    const char* buffer,
                                    uint32_t size) {
  // Get event info (operations and mapping).
  io::event_info const* info(io::events::instance().get_event_info(event_type));
  if (info) {
    // Create object.
    if (info->get_mapping()) {
      /* Two allocations here, object then control block: the constructor of the
       * operations table still hands back a raw pointer. Only the mapping path
       * -- BBDO2 -- goes through it, so it was left alone. */
      std::shared_ptr<io::data> t(info->get_operations().constructor());
      if (t) {
        t->source_id = source_id;
        t->destination_id = destination_id;
        // Browse all mapping to deserialize the object.
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
        return t;
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
      std::shared_ptr<io::data> t =
          info->get_operations().deserialize(buffer, size);
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
      return t;
    }
  } else {
    SPDLOG_LOGGER_INFO(
        _logger,
        "BBDO: cannot deserialize event of ID {}: event was not registered and "
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
 *  Write a BBDO packet header in place.
 *
 *  @param[out] header  Start of the BBDO_HEADER_SIZE bytes to fill.
 *  @param[in]  size    Size of the payload that follows.
 *  @param[in]  e       Event the packet carries.
 */
static void _fill_header(char* header, uint16_t size, const io::data& e) {
  *(reinterpret_cast<uint16_t*>(header + 2)) = htons(size);
  *(reinterpret_cast<uint32_t*>(header + 4)) = htonl(e.type());
  *(reinterpret_cast<uint32_t*>(header + 8)) = htonl(e.source_id);
  *(reinterpret_cast<uint32_t*>(header + 12)) = htonl(e.destination_id);
  *(reinterpret_cast<uint16_t*>(header)) =
      htons(misc::crc16_ccitt(header + 2, BBDO_HEADER_SIZE - 2));
}

/**
 *  Serialize an event in the BBDO protocol.
 *
 *  @param[in] e  Event to serialize.
 *
 *  @return Serialized event, null if the event type is not registered.
 *
 *  A shared_ptr and not a raw pointer: the only caller wraps the result in one
 *  anyway, and doing it there meant the object and its control block were two
 *  separate allocations. make_shared fuses them -- 52 170 control blocks over a
 *  120s load, 7.6% of everything cbd allocates, measured on EALLOC4. The two
 *  counts were identical in the trace, which is what identified the pattern.
 */
std::shared_ptr<io::raw> basic_stream::serialize(const io::data& e) {
  // Get event info (mapping).
  const io::event_info* info = io::events::instance().get_event_info(e.type());

  /* The protobuf events, which is what the monitoring flow is made of. The
   * buffer is allocated once and protobuf encodes straight into it, right
   * behind the header: 2 allocations and the single unavoidable copy
   *
   * Handled before the deque below, so that its map and its first node are not
   * paid either. */
  if (info && !info->get_mapping()) {
    auto buffer = std::make_shared<io::raw>();
    std::vector<char>& data(buffer->get_buffer());
    /* One allocation for the whole packet: serialize() sizes the buffer to the
     * header plus the body and writes the body behind the header. */
    const size_t size =
        info->get_operations().serialize(e, data, BBDO_HEADER_SIZE);

    if (size < 0xffff) {
      _fill_header(data.data(), size, e);
      return buffer;
    }

    /* A body spanning several packets. Rare — configuration, big metrics —
     * and reframed here from the bytes already encoded above rather than
     * serialized a second time.
     */
    std::vector<char> framed;
    framed.reserve(data.size() + (size / 0xffff) * BBDO_HEADER_SIZE);
    const char* body = data.data() + BBDO_HEADER_SIZE;
    size_t left = size;
    do {
      const size_t chunk = left < 0xffff ? left : 0xffff;
      const size_t header = framed.size();
      framed.resize(header + BBDO_HEADER_SIZE);
      framed.insert(framed.end(), body, body + chunk);
      _fill_header(framed.data() + header, chunk, e);
      body += chunk;
      left -= chunk;
    } while (left > 0);
    data = std::move(framed);
    return buffer;
  }

  std::deque<std::vector<char>> queue;

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
    }
    /* No else: an event without a mapping is a protobuf one, and those return
     * from the block at the top of this function. */

    // Finalization: concatenation of all the vectors in the queue.
    size_t size = 0;
    for (auto& v : queue)
      size += v.size();

    // Serialization buffer.
    std::shared_ptr<io::raw> buffer(std::make_shared<io::raw>());
    std::vector<char>& data(buffer->get_buffer());
    data.reserve(size);
    for (auto& v : queue)
      data.insert(data.end(), v.begin(), v.end());

    return buffer;
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
 * @brief Construct a new basic_stream::basic_stream object
 *
 * @param is_input true if we receive bbdo events such as broker input
 * @param grpc_serialized true if serialization is done by grpc stream only
 */
basic_stream::basic_stream(bool is_input, bool grpc_serialized)
    : io::stream("BBDO"),
      _skipped(0),
      _is_input{is_input},
      _coarse(false),
      _timeout(5),
      _acknowledged_events{0},
      _ack_limit(1000),
      _events_received_since_last_ack(0),
      _last_sent_ack(time(nullptr)),
      _grpc_serialized(grpc_serialized),
      _bbdo_version(config::applier::state::instance().get_bbdo_version()),
      _logger{log_v2::instance().get(log_v2::BBDO)} {
  SPDLOG_LOGGER_DEBUG(log_v2::instance().get(log_v2::CORE),
                      "create bbdo basic stream {:p}",
                      static_cast<const void*>(this));
}

/**
 * @brief Destroy the basic_stream::basic_stream object
 *
 */
basic_stream::~basic_stream() {
  SPDLOG_LOGGER_DEBUG(_logger, "destroy bbdo stream {}",
                      static_cast<void*>(this));
}

/**
 * @brief All the mecanism behind this stream is stopped once this method is
 * called. The last thing done is to return how many events are acknowledged.
 *
 * @return The number of events to acknowledge.
 */
uint32_t basic_stream::stop() {
  _logger->debug("bbdo::basic_stream stop {}", static_cast<void*>(this));
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
          "stopped: {}",
          e.what());
    }
  }

  /* We acknowledge peer about received events. */
  _logger->info("bbdo basic stream stopped with {} events acknowledged",
                _events_received_since_last_ack);
  if (_events_received_since_last_ack)
    send_event_acknowledgement();

  _substream->stop();

  /* We return the number of events handled by our basic stream. */
  uint32_t retval = _acknowledged_events;
  _acknowledged_events = 0;
  return retval;
}

/**
 *  Flush basic stream data.
 *
 *  @return Number of acknowledged events.
 */
uint32_t basic_stream::flush() {
  _substream->flush();
  uint32_t retval = _acknowledged_events;
  _acknowledged_events -= retval;
  return retval;
}

/**
 * @brief This method is called from a feeder stream. It is called when the
 * stream is goint soon to be stopped. It sends a stop message to the peer to
 * count how many events can be acknowledged.
 */
void basic_stream::_send_event_stop_and_wait_for_ack() {
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

std::list<std::string> basic_stream::get_running_config() {
  std::list<std::string> retval;
  std::shared_ptr<io::stream> substream = get_substream();
  while (substream) {
    retval.push_back(substream->get_name());
    substream = substream->get_substream();
  }
  return retval;
}

/**
 * @brief Handle a BBDO event. Events of category io::bbdo are the guardians
 * of BBDO messages. These messages are used by the protocol itself and are
 * always prioritized.
 *
 * @param d The event to handle.
 */
void basic_stream::_handle_bbdo_event(const std::shared_ptr<io::data>& d) {
  switch (d->type()) {
    // case version_response::static_type(): {
    //   auto version(std::static_pointer_cast<version_response>(d));
    //   if (version->bbdo_major != _bbdo_version.major_v) {
    //     SPDLOG_LOGGER_ERROR(
    //         _logger,
    //         "BBDO: peer is using protocol version {}.{}.{}, whereas we're "
    //         "using protocol version {}.{}.{}",
    //         version->bbdo_major, version->bbdo_minor, version->bbdo_patch,
    //         _bbdo_version.major_v, _bbdo_version.minor_v,
    //         _bbdo_version.patch);
    //     throw msg_fmt(
    //         "BBDO: peer is using protocol version {}.{}.{} "
    //         "whereas we're using protocol version {}.{}.{}",
    //         version->bbdo_major, version->bbdo_minor, version->bbdo_patch,
    //         _bbdo_version.major_v, _bbdo_version.minor_v,
    //         _bbdo_version.patch);
    //   }
    //   SPDLOG_LOGGER_INFO(
    //       _logger,
    //       "BBDO: peer is using protocol version {}.{}.{} , we're using "
    //       "version "
    //       "{}.{}.{}",
    //       version->bbdo_major, version->bbdo_minor, version->bbdo_patch,
    //       _bbdo_version.major_v, _bbdo_version.minor_v, _bbdo_version.patch);

    //  break;
    //}
    // case pb_welcome::static_type(): {
    //  auto welcome(std::static_pointer_cast<pb_welcome>(d));
    //  const auto& pb_version = welcome->obj().version();
    //  if (pb_version.major() != _bbdo_version.major_v) {
    //    SPDLOG_LOGGER_ERROR(
    //        _logger,
    //        "BBDO: peer is using protocol version {}.{}.{}, whereas we're "
    //        "using protocol version {}.{}.{}",
    //        pb_version.major(), pb_version.minor(), pb_version.patch(),
    //        _bbdo_version.major_v, _bbdo_version.minor_v,
    //        _bbdo_version.patch);
    //    throw msg_fmt(
    //        "BBDO: peer is using protocol version {}.{}.{} "
    //        "whereas we're using protocol version {}.{}.{}",
    //        pb_version.major(), pb_version.minor(), pb_version.patch(),
    //        _bbdo_version.major_v, _bbdo_version.minor_v,
    //        _bbdo_version.patch);
    //  }
    //  SPDLOG_LOGGER_INFO(
    //      _logger,
    //      "BBDO: peer is using protocol version {}.{}.{} , we're using "
    //      "version "
    //      "{}.{}.{}",
    //      pb_version.major(), pb_version.minor(), pb_version.patch(),
    //      _bbdo_version.major_v, _bbdo_version.minor_v, _bbdo_version.patch);
    //  break;
    //}
    // case ack::static_type():
    //  SPDLOG_LOGGER_INFO(
    //      _logger, "BBDO: received acknowledgement for {} events",
    //      std::static_pointer_cast<const ack>(d)->acknowledged_events);
    //  acknowledge_events(
    //      std::static_pointer_cast<const ack>(d)->acknowledged_events);
    //  break;
    // case pb_ack::static_type():
    //  SPDLOG_LOGGER_INFO(_logger,
    //                     "BBDO: received pb acknowledgement for {} events",
    //                     std::static_pointer_cast<const pb_ack>(d)
    //                         ->obj()
    //                         .acknowledged_events());
    //  acknowledge_events(std::static_pointer_cast<const pb_ack>(d)
    //                         ->obj()
    //                         .acknowledged_events());
    //  break;
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
    // case pb_diff_state::static_type(): {
    //   config::applier::state::instance().set_diff_state(d);
    // } break;
    // case pb_diff_state_ack::static_type(): {
    //   auto& obj = std::static_pointer_cast<pb_diff_state_ack>(d)->obj();
    //   assert(obj.poller_id() == _poller_id);
    //   config::applier::state::instance().set_poller_engine_conf(
    //       _poller_id, _poller_name, _broker_name, obj.config_version());
    //   config::applier::state::instance().acknowledge_engine_peer(
    //       obj.poller_id());
    //   SPDLOG_LOGGER_INFO(
    //       _logger,
    //       "BBDO: received diff state ack from poller {} with version '{}'",
    //       obj.poller_id(), obj.config_version());
    //   std::filesystem::path new_name(
    //       config::applier::state::instance().pollers_config_dir() /
    //       fmt::format("new-{}.prot", _poller_id));
    //   std::filesystem::path name(
    //       config::applier::state::instance().pollers_config_dir() /
    //       fmt::format("{}.prot", _poller_id));
    //   _logger->debug("bbdo::basic_stream removing {}", name.string());
    //   std::error_code ec;
    //   std::filesystem::rename(new_name, name, ec);
    //   if (ec)
    //     _logger->error("Unable to rename the file from '{}' to '{}'",
    //                    new_name.string(), name.string());

    //  // All the peer pollers have their configuration acknowledged.
    //  if (config::applier::state::instance().all_engine_peers_acknowledged())
    //  {
    //    SPDLOG_LOGGER_INFO(
    //        _logger,
    //        "BBDO: all engine peers have acknowledged their configuration");
    //    com::centreon::engine::configuration::indexed_diff_state global_diff;
    //    std::error_code ec;
    //    for (const auto& entry : std::filesystem::directory_iterator(
    //             config::applier::state::instance().pollers_config_dir(), ec))
    //             {
    //      std::string poller_id_str(entry.path().filename().string());
    //      if (entry.is_regular_file() && entry.path().extension() == ".prot"
    //      &&
    //          absl::StartsWith(poller_id_str, "diff-")) {
    //        _logger->debug("BBDO: Merging diff file '{}' into the global one",
    //                       entry.path().string());
    //        std::string_view poller_id_view(poller_id_str);
    //        poller_id_view.remove_prefix(5);
    //        poller_id_view.remove_suffix(5);
    //        uint64_t poller_id;
    //        if (absl::SimpleAtoi(poller_id_view, &poller_id)) {
    //          std::filesystem::path diff_name(
    //              config::applier::state::instance().pollers_config_dir() /
    //              entry.path());
    //          std::ifstream f(diff_name);
    //          com::centreon::engine::configuration::DiffState diff;
    //          if (f) {
    //            diff.ParseFromIstream(&f);
    //            f.close();
    //            global_diff.add_diff_state(diff, _logger);
    //            _logger->debug("BBDO: Removing diff file '{}'",
    //                           diff_name.string());
    //            std::filesystem::remove(diff_name);
    //          }
    //        } else {
    //          _logger->error(
    //              "BBDO: The file '{}' seems not to be a diff state file.",
    //              poller_id_str);
    //        }
    //      }
    //    }
    //    auto diff = std::make_shared<neb::pb_global_diff_state>();
    //    auto& obj = diff->mut_obj();
    //    global_diff.release_diff_state(obj);
    //    multiplexing::publisher pblshr;
    //    _logger->debug("BBDO: Publishing global diff state");
    //    pblshr.write(diff);
    //  }
    //} break;
    default:
      assert(false);
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
bool basic_stream::read(std::shared_ptr<io::data>& d, time_t deadline) {
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
   *  * an event has been returned but we could not deserialize it.
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
bool basic_stream::_read_any(std::shared_ptr<io::data>& d, time_t deadline) {
  try {
    // Return value.
    std::unique_ptr<io::data> e;
    d.reset();

    for (;;) {
      /* Maybe we have to complete the header. */
      if (!_read_packet(BBDO_HEADER_SIZE, deadline))
        return false;
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

      if (!_read_packet(BBDO_HEADER_SIZE + packet_size, deadline))
        return false;

      // Now, _packet contains at least BBDO_HEADER_SIZE + packet_size bytes.

      /* Where the body is, and how many bytes of _packet this event takes
       * with its header. */
      const char* body = _packet.data() + BBDO_HEADER_SIZE;
      size_t body_size = packet_size;
      const size_t eaten = BBDO_HEADER_SIZE + packet_size;

      /* The body is copied out of _packet only when it has to outlive it:
       * a full packet, which is the first part of a long event and will wait in
       * _buffer, or the last part of one, which is about to be concatenated
       * with what waits there. The ordinary case -- a whole event in one packet
       * -- is deserialized where the bytes landed, and _packet is consumed
       * afterwards. Materialising it every time was an allocation and a copy of
       * the whole body per event, for nothing. */
      const auto pending = absl::c_find_if(_buffer, [&](const buffer& b) {
        return b.matches(event_id, source_id, dest_id);
      });
      const bool must_copy =
          packet_size == 0xffff || pending != _buffer.end();

      std::vector<char> content;
      if (must_copy) {
        content.assign(body, body + packet_size);
        body = content.data();
        _drop_from_packet(eaten);
      }

      if (packet_size != 0xffff) {
        // Cool we can work with it!

        /* The last part of a long event: glue it behind the parts that were
         * waiting. pending was looked up above, when deciding whether the
         * body needed a copy of its own. */
        if (pending != _buffer.end()) {
          pending->push_back(std::move(content));
          content = pending->to_vector();
          _buffer.erase(pending);
          body = content.data();
          body_size = content.size();
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

        pack = body;

        // Maybe it is bigger now.
        packet_size = body_size;
        /* _packet is consumed after the deserialisation, because the bytes
         * being parsed may be its own -- but it has to be consumed whatever
         * happens: deserialize() throws on a body it cannot parse, and
         * leaving those bytes in place would have the next read pick the very
         * same header up again, forever. */
        try {
          d = deserialize(event_id, source_id, dest_id, pack, packet_size);
        } catch (...) {
          if (!must_copy)
            _drop_from_packet(eaten);
          throw;
        }
        if (!must_copy)
          _drop_from_packet(eaten);
        if (d) {
          SPDLOG_LOGGER_TRACE(_logger,
                              "deserialized {} bytes for event of type {}",
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
    /* Kept even though _read_packet no longer throws: a compression substream
     * still reports its own expired deadline this way, and that one is rare
     * enough to leave alone. */
    return false;
  }
  return false;
}

/**
 * @brief Drop the first bytes of _packet, those of an event just handled.
 *
 * Split out because the body is now deserialized straight out of _packet in
 * the ordinary case, so the packet cannot be consumed until after that: the two
 * call sites sit on either side of the deserialisation.
 *
 * @param size How many bytes to drop, header included.
 */
void basic_stream::_drop_from_packet(size_t size) {
  if (_packet.size() == size) {
    SPDLOG_LOGGER_TRACE(_logger,
                        "packet matched header + content, {} bytes consumed",
                        size);
    _packet.clear();
  } else {
    /* _packet.size() > size: it carried more than one BBDO packet. */
    SPDLOG_LOGGER_TRACE(_logger,
                        "packet longer than header + content: {} bytes "
                        "consumed out of {}, {} left",
                        size, _packet.size(), _packet.size() - size);
    _packet.erase(_packet.begin(), _packet.begin() + size);
  }
}

/**
 * @brief Fill the internal _packet vector until it reaches the given size. It
 * may be bigger. Even when the deadline expires the vector may begin to be
 * fill, it is just not finished, and so no data are lost. Received packets are
 * BBDO packets or maybe pieces of BBDO packets, so we keep vectors as is
 * because usually a vector should just represent a packet. In case of event
 * serialized only by grpc stream, we store it in _grpc_serialized_queue
 *
 * This used to report the expired deadline by throwing exceptions::timeout,
 * caught one frame above to do exactly what returning false does here. Having
 * nothing to read is the *ordinary* case on a live socket -- 5.4 times per
 * event, 424 times per second, measured on the EALLOC4 profile -- so that put a
 * heap allocation, a two-phase stack unwind and the lock guarding libgcc's
 * unwind tables on the hot path, the last one being a serialisation point
 * between the many threads of cbd.
 *
 * @param size The wanted final size
 * @param deadline A time_t.
 *
 * @return False if the deadline expired before the wanted size was reached,
 * true otherwise -- including when an already deserialized event was queued,
 * which the caller checks for right away.
 */
bool basic_stream::_read_packet(size_t size, time_t deadline) {
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
        return true;
      }
    }
    if (timeout) {
      SPDLOG_LOGGER_TRACE(_logger,
                          "_read_packet timeout!!, size = {}, deadline = {}",
                          size, deadline);
      return false;
    }
  }
  return true;
}

/**
 *  Set the limit of events received before an ack should be sent.
 *
 *  @param limit  The limit of events received before an ack should be sent.
 */
void basic_stream::set_ack_limit(uint32_t limit) {
  _ack_limit = limit;
}

/**
 *  Set whether this stream is coarse or not.
 *
 *  @param[in] coarse  True if coarse.
 */
void basic_stream::set_coarse(bool coarse) {
  _coarse = coarse;
}

/**
 *  Set the timeout supported by this stream.
 *
 *  @param[in] timeout  Timeout in seconds.
 */
void basic_stream::set_timeout(int timeout) {
  _timeout = timeout;
}

/**
 *  Get statistics.
 *
 *  @param[out] tree Output tree.
 */
void basic_stream::statistics(nlohmann::json& tree) const {
  tree["bbdo_input_ack_limit"] = static_cast<double>(_ack_limit);
  tree["bbdo_unacknowledged_events"] =
      static_cast<double>(_events_received_since_last_ack);

  if (_substream)
    _substream->statistics(tree);
}

void basic_stream::_write(const std::shared_ptr<io::data>& d) {
  assert(d);

  if (!_grpc_serialized || !std::dynamic_pointer_cast<io::protobuf_base>(d)) {
    std::shared_ptr<io::raw> serialized = serialize(*d);
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
uint32_t basic_stream::write(std::shared_ptr<io::data> const& d) {
  _write(d);

  uint32_t retval = _acknowledged_events;
  _acknowledged_events -= retval;
  return retval;
}

/**
 *  Acknowledge a certain amount of events.
 *
 *  @param[in] events  The amount of event.
 */
void basic_stream::acknowledge_events(uint32_t events) {
  _acknowledged_events += events;
}

/**
 *  Send an acknowledgement for all the events received.
 */
void basic_stream::send_event_acknowledgement() {
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

const bbdo::bbdo_version& basic_stream::get_bbdo_version() const {
  return _bbdo_version;
}

}  // namespace com::centreon::broker::bbdo
