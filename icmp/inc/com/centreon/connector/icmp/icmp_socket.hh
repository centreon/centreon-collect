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

#ifndef CCC_ICMP_ICMP_SOCKET_HH
#  define CCC_ICMP_ICMP_SOCKET_HH

#  include "com/centreon/connector/icmp/host.hh"
#  include "com/centreon/connector/icmp/namespace.hh"
#  include "com/centreon/connector/icmp/packet.hh"
#  include "com/centreon/handle.hh"

CCC_ICMP_BEGIN()

/**
 *  @class icmp_socket icmp_socket.hh "com/centreon/connector/icmp/icmp_socket.hh"
 *  @brief Implementation of icmp socket.
 *
 *  This class is an implementation of handle to use icmp socket.
 */
class           icmp_socket : public handle {
public:
                icmp_socket();
                ~icmp_socket() throw ();
  void          close();
  unsigned int  get_address() const throw ();
  native_handle get_native_handle();
  unsigned char get_ttl() const;
  unsigned long read(void* data, unsigned long size);
  void          set_address(unsigned int address) throw ();
  void          set_ttl(unsigned char ttl);
  unsigned long write(void const* data, unsigned long size);

private:
                icmp_socket(icmp_socket const& right);
  icmp_socket&  operator=(icmp_socket const& right);
  icmp_socket&  _internal_copy(icmp_socket const& right);

  unsigned int  _address;
  native_handle _internal_handle;
};

CCC_ICMP_END()

#endif // !CCC_ICMP_ICMP_SOCKET_HH
