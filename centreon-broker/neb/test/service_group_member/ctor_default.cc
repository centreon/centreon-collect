/*
** Copyright 2012-2013,2015 Merethis
**
** This file is part of Centreon Broker.
**
** Centreon Broker is free software: you can redistribute it and/or
** modify it under the terms of the GNU General Public License version 2
** as published by the Free Software Foundation.
**
** Centreon Broker is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
** General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with Centreon Broker. If not, see
** <http://www.gnu.org/licenses/>.
*/

#include <cstdlib>
#include "com/centreon/broker/io/events.hh"
#include "com/centreon/broker/neb/internal.hh"
#include "com/centreon/broker/neb/service_group_member.hh"

using namespace com::centreon::broker;

/**
 *  Check service_group_member's default constructor.
 *
 *  @return EXIT_SUCCESS on success.
 */
int main() {
  // Object.
  neb::service_group_member sgrpmmbr;

  // Check.
  return (((sgrpmmbr.source_id != 0)
           || (sgrpmmbr.destination_id != 0)
           || (sgrpmmbr.enabled != true)
           || (sgrpmmbr.group != "")
           || (sgrpmmbr.host_id != 0)
           || (sgrpmmbr.service_id != 0)
           || (sgrpmmbr.type()
               != io::events::data_type<
                                io::events::neb,
                                neb::de_service_group_member>::value))
          ? EXIT_FAILURE
          : EXIT_SUCCESS);
}
