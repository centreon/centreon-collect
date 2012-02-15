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

#ifndef CCC_ICMP_HOST_HH
#  define CCC_ICMP_HOST_HH

#  include <list>
#  include <string>
#  include "com/centreon/connector/icmp/namespace.hh"
#  include "com/centreon/logging/temp_logger.hh"
#  include "com/centreon/timestamp.hh"

CCC_ICMP_BEGIN()

/**
 *  @class host host.hh "com/centreon/connector/icmp/host.hh"
 *  @brief Store host information.
 *
 *  This class provide host infomation.
 */
class                     host {
public:
                          host(
                            std::string const& name,
                            unsigned int address);
                          host(host const& right);
                          ~host() throw ();
  host&                   operator=(host const& right);
  static char const*      address_to_string(unsigned int address);
  static void             factory(
                            std::string const& name,
                            std::list<host*>& hosts);
  unsigned int            get_address() const throw ();
  unsigned int            get_id() const throw ();
  std::string const&      get_error() const throw ();
  std::string const&      get_name() const throw ();
  unsigned int            get_packet_lost() const throw ();
  unsigned int            get_packet_recv() const throw ();
  unsigned int            get_packet_send() const throw ();
  unsigned int            get_roundtrip_avg() const throw ();
  unsigned int            get_roundtrip_max() const throw ();
  unsigned int            get_roundtrip_min() const throw ();
  unsigned int            get_total_time_waited() const throw ();
  void                    has_lost_packet(unsigned int elapsed_time) throw ();
  void                    has_recv_packet(unsigned int elapsed_time) throw ();
  void                    has_send_packet() throw ();
  void                    set_id(unsigned int id) throw ();
  void                    set_error(std::string const& error);

private:
  host&                   _internal_copy(host const& right);

  unsigned int            _address;
  std::string             _error;
  unsigned int            _id;
  std::string             _name;
  unsigned int            _packet_lost;
  unsigned int            _packet_recv;
  unsigned int            _packet_send;
  unsigned int            _roundtrip_max;
  unsigned int            _roundtrip_min;
  unsigned int            _total_time_waited;
};

logging::temp_logger operator<<(
                       logging::temp_logger log,
                       host const& right);

CCC_ICMP_END()

#endif // !CCC_ICMP_HOST_HH
