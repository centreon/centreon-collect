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

#include "com/centreon/connector/icmp/icmp_info.hh"

using namespace com::centreon::connector::icmp;

/**
 *  Default constructor.
 *
 *  @param[in, out] chk  The check.
 *  @param[in, out] hst  The host.
 *  @param[in, out] pkt  The packet.
 */
icmp_info::icmp_info(check* _chk, host* _hst, packet* _pkt)
  : chk(_chk), hst(_hst), pkt(_pkt) {

}

/**
 *  Default copy constructor.
 *
 *  @param[in] right  The object to copy.
 */
icmp_info::icmp_info(icmp_info const& right) {
  _internal_copy(right);
}

/**
 *  Default destructor.
 */
icmp_info::~icmp_info() throw () {

}

/**
 *  Defautl copy operator.
 *
 *  @param[in] right  The object to copy.
 *
 *  @return This object.
 */
icmp_info& icmp_info::operator=(icmp_info const& right) {
  return (_internal_copy(right));
}

/**
 *  Internal copy.
 *
 *  @param[in] right  The object to copy.
 *
 *  @return This object.
 */
icmp_info& icmp_info::_internal_copy(icmp_info const& right) {
  if (this != &right) {
    chk = right.chk;
    hst = right.hst;
    pkt = right.pkt;
  }
  return (*this);
}

