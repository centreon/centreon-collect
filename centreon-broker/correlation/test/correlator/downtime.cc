/*
** Copyright 2011-2013 Merethis
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
#include <iostream>
#include <QMap>
#include <QPair>
#include "com/centreon/broker/config/applier/init.hh"
#include "com/centreon/broker/correlation/correlator.hh"
#include "com/centreon/broker/correlation/node.hh"
#include "com/centreon/broker/neb/service_status.hh"
#include "test/correlator/common.hh"

using namespace com::centreon::broker;
using namespace com::centreon::broker::correlation;

/**
 *  Check that node is properly default constructed.
 *
 *  @return EXIT_SUCCESS on success.
 */
int main() {
  // Return value.
  int retval(EXIT_FAILURE);

  // Initialization.
  config::applier::init();

  try {
    // Create state.
    QMap<QPair<unsigned int, unsigned int>, node> state;
    node& n(state[qMakePair(42u, 24u)]);
    n.host_id = 42;
    n.instance_id = 1;
    n.service_id = 24;
    n.state = 0;

    // Create correlator and apply state.
    correlator c(0);
    c.set_state(state);

    // Send node status.
    { // #1
      misc::shared_ptr<neb::service_status> ss(new neb::service_status);
      ss->host_id = 42;
      ss->instance_id = 1;
      ss->service_id = 24;
      ss->state_type = 1;
      ss->current_state = 2;
      ss->last_check = 123456789;
      ss->last_update = 123456789;
      c.write(ss.staticCast<io::data>());
    }
    { // #2
      misc::shared_ptr<neb::service_status> ss(new neb::service_status);
      ss->host_id = 42;
      ss->instance_id = 1;
      ss->service_id = 24;
      ss->state_type = 1;
      ss->current_state = 2;
      ss->last_check = 123456790;
      ss->last_update = 123456790;
      ss->scheduled_downtime_depth = 1;
      c.write(ss.staticCast<io::data>());
    }
    { // #3
      misc::shared_ptr<neb::service_status> ss(new neb::service_status);
      ss->host_id = 42;
      ss->instance_id = 1;
      ss->service_id = 24;
      ss->state_type = 1;
      ss->current_state = 0;
      ss->last_check = 123456791;
      ss->last_update = 123456791;
      c.write(ss.staticCast<io::data>());
    }
    { // #4
      misc::shared_ptr<neb::service_status> ss(new neb::service_status);
      ss->host_id = 42;
      ss->instance_id = 1;
      ss->service_id = 24;
      ss->state_type = 1;
      ss->current_state = 0;
      ss->last_check = 123456792;
      ss->last_update = 123456792;
      ss->scheduled_downtime_depth = 1;
      c.write(ss.staticCast<io::data>());
    }
    { // #5
      misc::shared_ptr<neb::service_status> ss(new neb::service_status);
      ss->host_id = 42;
      ss->instance_id = 1;
      ss->service_id = 24;
      ss->state_type = 1;
      ss->current_state = 2;
      ss->last_check = 123456793;
      ss->last_update = 123456793;
      ss->scheduled_downtime_depth = 1;
      c.write(ss.staticCast<io::data>());
    }
    { // #6
      misc::shared_ptr<neb::service_status> ss(new neb::service_status);
      ss->host_id = 42;
      ss->instance_id = 1;
      ss->service_id = 24;
      ss->state_type = 1;
      ss->current_state = 0;
      ss->last_check = 123456794;
      ss->last_update = 123456794;
      ss->scheduled_downtime_depth = 1;
      c.write(ss.staticCast<io::data>());
    }
    { // #7
      misc::shared_ptr<neb::service_status> ss(new neb::service_status);
      ss->host_id = 42;
      ss->instance_id = 1;
      ss->service_id = 24;
      ss->state_type = 1;
      ss->current_state = 0;
      ss->last_check = 123456795;
      ss->last_update = 123456795;
      ss->scheduled_downtime_depth = 0;
      c.write(ss.staticCast<io::data>());
    }

    // Check correlation content.
    QList<misc::shared_ptr<io::data> > content;
    // #1
    add_state_service(content, -1, 0, 123456789, 42, 1, false, 24, 0);
    add_state_service(content, -1, 2, 0, 42, 1, false, 24, 123456789);
    add_issue(content, 0, 0, 42, 1, 24, 123456789);
    // #2
    add_state_service(
      content,
      -1,
      2,
      123456790,
      42,
      1,
      false,
      24,
      123456789);
    add_state_service(content, -1, 2, 0, 42, 1, true, 24, 123456790);
    // #3
    add_state_service(
      content,
      -1,
      2,
      123456791,
      42,
      1,
      true,
      24,
      123456790);
    add_state_service(content, -1, 0, 0, 42, 1, false, 24, 123456791);
    add_issue(content, 0, 123456791, 42, 1, 24, 123456789);
    // #4
    add_state_service(
      content,
      -1,
      0,
      123456792,
      42,
      1,
      false,
      24,
      123456791);
    add_state_service(content, -1, 0, 0, 42, 1, true, 24, 123456792);
    // #5
    add_state_service(
      content,
      -1,
      0,
      123456793,
      42,
      1,
      true,
      24,
      123456792);
    add_state_service(content, -1, 2, 0, 42, 1, true, 24, 123456793);
    add_issue(content, 0, 0, 42, 1, 24, 123456793);
    // #6
    add_state_service(
      content,
      -1,
      2,
      123456794,
      42,
      1,
      true,
      24,
      123456793);
    add_state_service(content, -1, 0, 0, 42, 1, true, 24, 123456794);
    add_issue(content, 0, 123456794, 42, 1, 24, 123456793);
    // #7
    add_state_service(
      content,
      -1,
      0,
      123456795,
      42,
      1,
      true,
      24,
      123456794);
    add_state_service(content, -1, 0, 0, 42, 1, false, 24, 123456795);

    // Check.
    check_content(c, content);

    // Success.
    retval = EXIT_SUCCESS;
  }
  catch (std::exception const& e) {
    std::cout << e.what() << std::endl;
  }
  catch (...) {
    std::cout << "unknown exception" << std::endl;
  }

  return (retval);
}
