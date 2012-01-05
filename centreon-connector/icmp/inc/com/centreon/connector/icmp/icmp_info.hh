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

#ifndef CCC_ICMP_ICMP_INFO_HH
#  define CCC_ICMP_ICMP_INFO_HH

#  include "com/centreon/connector/icmp/check.hh"
#  include "com/centreon/connector/icmp/host.hh"
#  include "com/centreon/connector/icmp/namespace.hh"
#  include "com/centreon/connector/icmp/packet.hh"

CCC_ICMP_BEGIN()

/**
 *  @class icmp_info icmp_info.hh "com/centreon/connector/icmp/icmp_info.hh"
 *  @brief Provide information for icmp check.
 *
 *  This class provide all information for a icmp check.
 */
struct       icmp_info {
public:
             icmp_info(
               check* chk = NULL,
               host* hst = NULL,
               packet* pkt = NULL);
             icmp_info(icmp_info const& right);
             ~icmp_info() throw ();
  icmp_info& operator=(icmp_info const& right);

  check*     chk;
  host*      hst;
  packet*    pkt;

private:
  icmp_info& _internal_copy(icmp_info const& right);
};

CCC_ICMP_END()

#endif // !CCC_ICMP_ICMP_INFO_HH
