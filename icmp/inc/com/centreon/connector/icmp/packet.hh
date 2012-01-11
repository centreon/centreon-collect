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

#ifndef CCC_ICMP_PACKET_HH
#  define CCC_ICMP_PACKET_HH

#  include "com/centreon/connector/icmp/namespace.hh"
#  include "com/centreon/logging/temp_logger.hh"
#  include "com/centreon/timestamp.hh"

CCC_ICMP_BEGIN()

/**
 *  @class packet packet.hh "com/centreon/connector/icmp/packet.hh"
 *  @brief Wrap an icmp packet.
 *
 *  This class provide a wrapper to icmp packet.
 */
class                    packet {
public:
  enum                   icmp_type {
    icmp_echo = 0,
    icmp_echoreply = 1,
    unkonwn = 255
  };

                         packet();
                         packet(
                           unsigned char const* data,
                           unsigned short size,
                           timestamp const& time);
                         packet(unsigned short size);
                         packet(packet const& right);
                         ~packet() throw ();
  packet&                operator=(packet const& right);
  unsigned int           get_address() const throw ();
  unsigned char          get_code() const throw ();
  void const*            get_data() const throw ();
  char const*            get_error() const throw ();
  unsigned int           get_host_id() const throw ();
  unsigned short         get_id() const throw ();
  unsigned short         get_sequence() const throw ();
  unsigned short         get_size() const throw ();
  timestamp const&       get_recv_time() const;
  timestamp              get_send_time() const;
  icmp_type              get_type() const throw ();
  void                   set_address(unsigned int address) throw ();
  void                   set_code(char code) throw ();
  void                   set_host_id(unsigned int host_id) throw ();
  void                   set_id(unsigned short id) throw ();
  void                   set_sequence(unsigned short seq) throw ();
  void                   set_type(icmp_type type) throw ();

private:
  unsigned short         _checksum_icmp() const throw ();
  packet&                _internal_copy(packet const& right);

  unsigned int           _address;
  unsigned char*         _buffer;
  unsigned short         _size;
  timestamp              _recv_time;
};

logging::temp_logger operator<<(
                       logging::temp_logger log,
                       packet const& right);

CCC_ICMP_END()

#endif // !CCC_ICMP_PACKET_HH
