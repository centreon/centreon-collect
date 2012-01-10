/*
** Copyright 2011-2012 Merethis
**
** This file is part of Centreon Connector ICMP.
**
** Centreon Connector ICMP is free software: you can redistribute it
** and/or modify it under the terms of the GNU Affero General Public
** License as published by the Free Software Foundation, either version
** 3 of the License, or (at your option) any later version.
**
** Centreon Connector ICMP is distributed in the hope that it will be
** useful, but WITHOUT ANY WARRANTY; without even the implied warranty
** of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
** Affero General Public License for more details.
**
** You should have received a copy of the GNU Affero General Public
** License along with Centreon Connector ICMP. If not, see
** <http://www.gnu.org/licenses/>.
*/

#include <arpa/inet.h>
#include <netinet/ip_icmp.h>
#include <string.h>
#include "com/centreon/exceptions/basic.hh"
#include "com/centreon/connector/icmp/host.hh"
#include "com/centreon/connector/icmp/packet.hh"

using namespace com::centreon::connector::icmp;
using namespace com::centreon;

struct error_message {
  unsigned int id;
  char const*  msg;
};

static error_message gl_unreach_msg[] = {
  { ICMP_UNREACH_NET, "Net unreachable" },
  { ICMP_UNREACH_HOST, "Host unreachable" },
  { ICMP_UNREACH_PROTOCOL, "Protocol unreachable" },
  { ICMP_UNREACH_PORT, "Port unreachable" },
  { ICMP_UNREACH_NEEDFRAG, "Fragmentation needed" },
  { ICMP_UNREACH_SRCFAIL, "Source route failed" },
  { ICMP_UNREACH_NET_UNKNOWN, "Unknown network" },
  { ICMP_UNREACH_HOST_UNKNOWN, "Unknown host" },
  { ICMP_UNREACH_ISOLATED, "Source host isolated" },
  { ICMP_UNREACH_NET_PROHIB, "Network denied" },
  { ICMP_UNREACH_HOST_PROHIB, "Host denied" },
  { ICMP_UNREACH_TOSNET, "Bad TOS for network" },
  { ICMP_UNREACH_TOSHOST, "Bad TOS for host" },
  { ICMP_UNREACH_FILTER_PROHIB, "Prohibited by filter" },
  { ICMP_UNREACH_HOST_PRECEDENCE, "Host precedence violation" },
  { ICMP_UNREACH_PRECEDENCE_CUTOFF, "Precedence cutoff" }
};

static error_message gl_timxceed_msg[] = {
  { ICMP_TIMXCEED_INTRANS, "Time to live exceeded in transit" },
  { ICMP_TIMXCEED_REASS, "Fragment reassembly time exceeded" }
};

/**
 *  Default constructor.
 */
packet::packet()
  : _address(0),
    _buffer(NULL),
    _size(0){
}

/**
 *  Constructor to build packet with raw data.
 *
 *  @param[in] data  Raw data.
 *  @param[in] size  Data size.
 *  @param[in] time  Time when the packet was receive.
 */
packet::packet(
          unsigned char const* data,
          unsigned short size,
          timestamp const& time)
  : _address(0),
    _buffer(NULL),
    _size(size),
    _recv_time(time) {
  if (!data)
    throw (basic_error() << "invalid argument:null pointer");
  if (!_size
      || _size < sizeof(::icmp) + sizeof(long long) + sizeof(unsigned int))
    throw (basic_error() << "invalid packet size");
  _buffer = new unsigned char[size];
  memcpy(_buffer, data, _size * sizeof(*_buffer));
}

/**
 *  Constructor to create packet.
 *
 *  @param[in] size     The packet data size.
 */
packet::packet(unsigned short size)
  : _address(0),
    _buffer(NULL),
    _size(sizeof(::icmp) + sizeof(long long) + sizeof(unsigned int) + size) {
  if (_size < sizeof(::icmp) + sizeof(long long) + sizeof(unsigned int))
    throw (basic_error() << "invalid packet size");
  _buffer = new unsigned char[_size];
  memset(_buffer, 0, _size * sizeof(*_buffer));
  set_type(icmp_echo);
}

/**
 *  Default copy constructor.
 *
 *  @param[in] right  The object to copy.
 */
packet::packet(packet const& right)
  : _address(0),
    _buffer(NULL),
    _size(0) {
  _internal_copy(right);
}

/**
 *  Default destructor.
 */
packet::~packet() throw () {
  delete[] _buffer;
}

/**
 *  Default copy operator.
 *
 *  @param[in] right  The object to copy.
 *
 *  @return This object.
 */
packet& packet::operator=(packet const& right) {
  return (_internal_copy(right));
}

/**
 *  Get the packet address.
 *
 *  @return The address.
 */
unsigned int packet::get_address() const throw () {
  return (_address);
}

/**
 *  Get the packet code.
 *
 *  @return The code.
 */
unsigned char packet::get_code() const throw () {
  return (reinterpret_cast<struct icmp*>(_buffer)->icmp_code);
}

/**
 *  Get the packet content.
 *
 *  @return The packet data.
 */
void const* packet::get_data() const throw () {
  ::icmp* pkt(reinterpret_cast<struct icmp*>(_buffer));
  long long now(timestamp::now().to_useconds());
  memcpy(&pkt->icmp_data,
         &now,
         sizeof(long long));
  pkt->icmp_cksum = _checksum_icmp();
  return (_buffer);
}

/**
 *  Get the packet error message.
 *
 *  @return The string message.
 */
char const* packet::get_error() const throw () {
  ::icmp const* pkt(reinterpret_cast<struct icmp const*>(_buffer));
  unsigned char type(pkt->icmp_type);
  unsigned char code(pkt->icmp_code);

  char const* msg("");
  switch (type) {
  case ICMP_UNREACH:
    msg = "Invalid code";
    for (unsigned int i(0); i < sizeof(gl_unreach_msg); ++i)
      if (gl_unreach_msg[i].id == code) {
        msg = gl_unreach_msg[i].msg;
        break;
      }
    break;

  case ICMP_TIMXCEED:
    msg = "Invalid code";
    for (unsigned int i(0); i < sizeof(gl_timxceed_msg); ++i)
      if (gl_timxceed_msg[i].id == code) {
        msg = gl_timxceed_msg[i].msg;
        break;
      }
    break;

  case ICMP_SOURCEQUENCH:
    msg = "Transmitting too fast";
    break;

  case ICMP_REDIRECT:
    msg = "Redirect (change route)";
    break;

  case ICMP_PARAMPROB:
    msg = "Bad IP header (required option absent)";
    break;
  }
  return (msg);
}

/**
 *  Get the packet host id.
 *
 *  @return The host id.
 */
unsigned int packet::get_host_id() const throw () {
  ::icmp* pkt(reinterpret_cast<struct icmp*>(_buffer));
  unsigned int id;
  memcpy(&id,
         &pkt->icmp_data + sizeof(long long),
         sizeof(id));
  return (ntohl(id));
}

/**
 *  Get the packet id.
 *
 *  @return The packet id.
 */
unsigned short packet::get_id() const throw () {
  return (ntohs(reinterpret_cast<struct icmp*>(_buffer)->icmp_id));
}

/**
 *  Get the packet sequence.
 *
 *  @return The packet sequence.
 */
unsigned short packet::get_sequence() const throw () {
  return (ntohs(reinterpret_cast<struct icmp*>(_buffer)->icmp_seq));
}

/**
 *  Get the packet data size.
 *
 *  @return The data size.
 */
unsigned short packet::get_size() const throw () {
  return (_size);
}

/**
 *  Get the packet type.
 *
 *  @return The packet type.
 */
packet::icmp_type packet::get_type() const throw () {
  unsigned char type(reinterpret_cast<struct icmp*>(_buffer)->icmp_type);
  if (type == ICMP_ECHO)
    return (icmp_echo);
  if (type == ICMP_ECHOREPLY)
    return (icmp_echoreply);
  return (unkonwn);
}

/**
 *  Get when the packet was receive.
 *
 *  @return the time.
 */
com::centreon::timestamp const& packet::get_recv_time() const {
  return (_recv_time);
}

/**
 *  Get when the packet was send.
 *
 *  @return the time.
 */
com::centreon::timestamp packet::get_send_time() const {
  ::icmp* pkt(reinterpret_cast<struct icmp*>(_buffer));
  long long time;
  memcpy(&time, &pkt->icmp_data, sizeof(long long));
  return (timestamp(0, time));
}

/**
 *  Set the packet address.
 *
 *  @param[in] id  The address.
 */
void packet::set_address(unsigned int address) throw () {
  _address = address;
}

/**
 *  Set the packet host id.
 *
 *  @param[in] id  The id.
 */
void packet::set_host_id(unsigned int id) throw () {
  ::icmp* pkt(reinterpret_cast<struct icmp*>(_buffer));
  id = htonl(id);
  memcpy(
    &pkt->icmp_data + sizeof(long long),
    &id,
    sizeof(id));
}

/**
 *  Set the packet code.
 *
 *  @param[in] code  The code.
 */
void packet::set_code(char code) throw () {
  reinterpret_cast<struct icmp*>(_buffer)->icmp_code = code;
}

/**
 *  Set the packet id.
 *
 *  @param[in] id  The packet id.
 */
void packet::set_id(unsigned short id) throw () {
  reinterpret_cast<struct icmp*>(_buffer)->icmp_id = ntohs(id);
}

/**
 *  Set the packet sequence number.
 *
 *  @param[in] seq  The sequence number.
 */
void packet::set_sequence(unsigned short seq) throw () {
  reinterpret_cast<struct icmp*>(_buffer)->icmp_seq = ntohs(seq);
}

/**
 *  Set the packet type.
 *
 *  @param[in] type  The type.
 */
void packet::set_type(icmp_type type) throw () {
  if (type == icmp_echo)
    reinterpret_cast<struct icmp*>(_buffer)->icmp_type = ICMP_ECHO;
  else if (type == icmp_echoreply)
    reinterpret_cast<struct icmp*>(_buffer)->icmp_type = ICMP_ECHOREPLY;
  else
    reinterpret_cast<struct icmp*>(_buffer)->icmp_type = unkonwn;
}

/**
 *  Calculate the checksum of the packet buffer.
 *
 *  @return The checksum of the packet buffer.
 */
unsigned short packet::_checksum_icmp() const throw () {
  long sum(0);
  for (unsigned int i(0), end(_size / 2); i < end; ++i)
    sum += reinterpret_cast<unsigned short*>(_buffer)[i];
  if (_size % 2)
    sum += _buffer[_size - 1];

  sum = (sum >> 16) + (sum & 0xFFFF);
  sum += (sum >> 16);

  return (~sum);
}

/**
 *  internal copy.
 *
 *  @param[in] right  The object to copy.
 *
 *  @return This object.
 */
packet& packet::_internal_copy(packet const& right) {
  if (this != &right) {
    if (_size != right._size) {
      _size = right._size;
      delete[] _buffer;
      if (_size) {
        _buffer = new unsigned char[_size];
        memcpy(_buffer, right._buffer, _size);
      }
      else
        _buffer = NULL;
    }
    else if (_buffer)
      memcpy(_buffer, right._buffer, _size);
    _address = right._address;
    _recv_time = right._recv_time;
  }
  return (*this);
}

/**
 *  Overload of redirectoion operator for temp_logger. Allow to log
 *  packet.
 *
 *  @param[out] log    The temp_logger to write data.
 *  @param[in]  right  The packet to log.
 */
logging::temp_logger connector::icmp::operator<<(
                                        logging::temp_logger log,
                                        packet const& right) {
  log << "packet (" << &right << ") {\n"
      << "  address:        "
      << host::address_to_string(right.get_address()) << "\n"
      << "  host id:        "
      << right.get_host_id() << "\n"
      << "  id:             "
      << right.get_id() << "\n"
      << "  type:           "
      << right.get_type() << "\n"
      << "  code:           "
      << right.get_code() << "\n"
      << "  sequence:       "
      << right.get_sequence() << "\n"
      << "  recv timestamp: "
      << right.get_recv_time().to_useconds() << "\n"
      << "  send timestamp: "
      << right.get_send_time().to_useconds() << "\n"
      << "  size:           "
      << right.get_size() << "\n"
      << "}";
  return (log);
}
