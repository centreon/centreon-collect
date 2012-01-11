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

#include <iostream>
#include <string.h>
#include "com/centreon/exceptions/basic.hh"
#include "com/centreon/connector/icmp/check.hh"

using namespace com::centreon::connector::icmp;
using namespace com::centreon;

/**
 *  Compare check.
 *
 *  @param[in] c1  The first object to compare.
 *  @param[in] c2  The second object to compare.
 *
 *  @return True if is the same, otherwize false.
 */
bool is_same(check const& c1, check const& c2) {
  if (c1.get_command_id() != c2.get_command_id()
      || c1.get_current_host_check() != c2.get_current_host_check()
      || c1.get_max_completion_time() != c2.get_max_completion_time()
      || c1.get_max_packet_interval() != c2.get_max_packet_interval()
      || c1.get_max_target_interval() != c2.get_max_target_interval()
      || c1.get_min_hosts_alive() != c2.get_min_hosts_alive()
      || c1.get_nb_packet() != c2.get_nb_packet()
      || c1.get_packet_data_size() != c2.get_packet_data_size()
      || c1.get_critical_packet_lost() != c2.get_critical_packet_lost()
      || c1.get_critical_roundtrip_avg() != c2.get_critical_roundtrip_avg()
      || c1.get_warning_packet_lost() != c2.get_warning_packet_lost()
      || c1.get_warning_roundtrip_avg() != c2.get_warning_roundtrip_avg())
    return (false);
  return (true);
}

/**
 *  Check check constructor.
 *
 *  @return 0 on success.
 */
int main() {
  try {
    check ref(42, "");
    ref.host_was_checked();

    check c1(ref);
    if (!is_same(ref, c1))
      throw (basic_error() << "copy constructor failed");

    check c2 = ref;
    if (!is_same(ref, c2))
      throw (basic_error() << "copy operator failed");
  }
  catch (std::exception const& e) {
    std::cerr << "error: " << e.what() << std::endl;
    return (1);
  }
  return (0);
}
