/*
** Copyright 2011 Merethis
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
#include <limits.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include "com/centreon/exceptions/basic.hh"
#include "com/centreon/connector/icmp/host.hh"

using namespace com::centreon::connector::icmp;
using namespace com::centreon;

/**
 *  Default constructor.
 *
 *  @param[in] address  The host address.
 */
host::host(std::string const& name, unsigned int address)
  : _address(address),
    _id(0),
    _name(name),
    _packet_lost(0),
    _packet_recv(0),
    _packet_send(0),
    _roundtrip_max(0),
    _roundtrip_min(UINT_MAX),
    _total_time_waited(0) {

}

/**
 *  Default copy constructor.
 *
 *  @param[in] right  The object to copy.
 */
host::host(host const& right) {
  _internal_copy(right);
}

/**
 *  Default destructor.
 */
host::~host() throw () {

}

/**
 *  Default copy operator.
 *
 *  @param[in] right  The object to copy.
 *
 *  @return This object.
 */
host& host::operator=(host const& right) {
  return (_internal_copy(right));
}

/**
 *  Get the host address to string.
 *
 *  @return The address into string.
 */
char const* host::address_to_string(unsigned int address) {
  return (inet_ntoa(*reinterpret_cast<in_addr const*>(&address)));
}

/**
 *  Build host with an host name.
 *
 *  @param[in] name  The host name.
 */
std::list<host*> host::factory(std::string const& name) {
  std::list<host*> hosts;
  unsigned int addr(inet_addr(name.c_str()));
  if (addr != INADDR_NONE)
    hosts.push_back(new host(name, addr));
  else {
    hostent* he(gethostbyname(name.c_str()));
    if (!he)
      throw (basic_error() << "create host failed:"
             << hstrerror(h_errno));

    for(unsigned int i(0); he->h_addr_list[i]; ++i) {
      in_addr* in(reinterpret_cast<in_addr*>(he->h_addr_list[i]));
      if (in->s_addr == INADDR_NONE || in->s_addr == INADDR_ANY)
        continue;
      hosts.push_back(new host(name, in->s_addr));
    }
  }
  return (hosts);
}

/**
 *  Get the host address.
 *
 *  @return The address.
 */
unsigned int host::get_address() const throw () {
  return (_address);
}

/**
 *  Get the host id.
 *
 *  @return The id.
 */
unsigned int host::get_id() const throw () {
  return (_id);
}

/**
 *  Get the error message.
 *
 *  @return The error message.
 */
std::string const& host::get_error() const throw () {
  return (_error);
}

/**
 *  Get the host name.
 *
 *  @return The name.
 */
std::string const& host::get_name() const throw () {
  return (_name);
}

/**
 *  Get the number of packet lost for this host.
 *
 *  @return The number of packet lost.
 */
unsigned int host::get_packet_lost() const throw () {
  return (_packet_lost);
}

/**
 *  Get the number of packet receive for this host.
 *
 *  @return The number of packet receive.
 */
unsigned int host::get_packet_recv() const throw () {
  return (_packet_recv);
}

/**
 *  Get the number of packet send by this host.
 *
 *  @return The number of packet send.
 */
unsigned int host::get_packet_send() const throw () {
  return (_packet_send);
}

/**
 *  Get the average of diff time to receive a packet.
 *
 *  @return The diff time.
 */
unsigned int host::get_roundtrip_avg() const throw () {
  return (_packet_recv ? _total_time_waited / _packet_send : 0);
}

/**
 *  Get the maximum of diff time to receive a packet.
 *
 *  @return The diff time.
 */
unsigned int host::get_roundtrip_max() const throw () {
  return (_packet_recv ? _roundtrip_max : 0);
}

/**
 *  Get the minimum of diff time to receive a packet.
 *
 *  @return The diff time.
 */
unsigned int host::get_roundtrip_min() const throw () {
  return (_packet_recv && _roundtrip_min != UINT_MAX
          ? _roundtrip_min
          : 0);
}

/**
 *  Get the total time to wait packets.
 *
 *  @return The time.
 */
unsigned int host::get_total_time_waited() const throw () {
  return (_total_time_waited);
}

/**
 *  The icmp packet was lost, update data.
 *
 *  @param[in] elapsed_time  The time elapsed between send and
 *             receive packet, in microsecond.
 */
void host::has_lost_packet(unsigned int elapsed_time) throw () {
  ++_packet_lost;
  _total_time_waited += elapsed_time;
  if (_roundtrip_max < elapsed_time)
    _roundtrip_max = elapsed_time;
  if (_roundtrip_min > elapsed_time)
    _roundtrip_min = elapsed_time;
}

/**
 *  The icmp packet was receive, update data.
 *
 *  @param[in] elapsed_time  The time elapsed between send and
 *             receive packet in microsecond.
 */
void host::has_recv_packet(unsigned int elapsed_time) throw () {
  ++_packet_recv;
  _total_time_waited += elapsed_time;
  if (_roundtrip_max < elapsed_time)
    _roundtrip_max = elapsed_time;
  if (_roundtrip_min > elapsed_time)
    _roundtrip_min = elapsed_time;
}

/**
 *  The icmp packet was send, update data.
 */
void host::has_send_packet() throw () {
  ++_packet_send;
}

/**
 *  Set the id.
 *
 *  @param[in] id  The id.
 */
void host::set_id(unsigned int id) throw () {
  _id = id;
}

/**
 *  Set the error message.
 *
 *  @param[in] error  The error message.
 */
void host::set_error(std::string const& error) {
  _error = error;
}

/**
 *  Internal copy.
 *
 *  @param[in] right  The object to copy.
 *
 *  @return This object.
 */
host& host::_internal_copy(host const& right) {
  if (this != &right) {
    _address = right._address;
    _error = right._error;
    _name = right._name;
    _packet_lost = right._packet_lost;
    _packet_recv = right._packet_recv;
    _packet_send = right._packet_send;
    _roundtrip_max = right._roundtrip_max;
    _roundtrip_min = right._roundtrip_min;
    _total_time_waited = right._total_time_waited;
  }
  return (*this);
}

/**
 *  Overload of redirectoion operator for temp_logger. Allow to log
 *  host.
 *
 *  @param[out] log    The temp_logger to write data.
 *  @param[in]  right  The host to log.
 */
logging::temp_logger connector::icmp::operator<<(
                                        logging::temp_logger log,
                                        host const& right) {
  log << "host (" << &right << ") {\n"
      << "  name:              " << right.get_name() << "\n"
      << "  address:           " << host::address_to_string(right.get_address()) << "\n"
      << "  id:                " << right.get_id() << "\n"
      << "  error:             \"" << right.get_error() << "\"\n"
      << "  packet_lost:       " << right.get_packet_lost() << "\n"
      << "  packet_recv:       " << right.get_packet_recv() << "\n"
      << "  packet_send:       " << right.get_packet_send() << "\n"
      << "  roundtrip_avg:     " << right.get_roundtrip_avg() << "\n"
      << "  roundtrip_max:     " << right.get_roundtrip_max() << "\n"
      << "  roundtrip_min:     " << right.get_roundtrip_min() << "\n"
      << "  total_time_waited: " << right.get_total_time_waited() << "\n"
      << "}";
  return (log);
}
