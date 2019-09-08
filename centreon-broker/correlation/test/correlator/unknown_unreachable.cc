/*
** Copyright 2011-2015 Centreon
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
**
** For more information : contact@centreon.com
*/

#include <QMap>
#include <QPair>
#include <cstdlib>
#include <iostream>
#include "com/centreon/broker/config/applier/init.hh"
#include "com/centreon/broker/correlation/issue.hh"
#include "com/centreon/broker/correlation/issue_parent.hh"
#include "com/centreon/broker/correlation/node.hh"
#include "com/centreon/broker/correlation/stream.hh"
#include "com/centreon/broker/multiplexing/engine.hh"
#include "com/centreon/broker/neb/host_status.hh"
#include "com/centreon/broker/neb/service_status.hh"
#include "test/correlator/common.hh"

using namespace com::centreon::broker;
using namespace com::centreon::broker::correlation;

/**
 *  Check that dependencies work.
 *
 *  @return EXIT_SUCCESS on success.
 */
int main() {
  // Return value.
  int retval(EXIT_FAILURE);

  // Initialization.
  config::applier::init();
  multiplexing::engine::load();
  // Start the multiplexing engine.
  test_stream t;
  multiplexing::engine::instance().hook(t);
  multiplexing::engine::instance().start();

  try {
    correlation::stream c("", std::shared_ptr<persistent_cache>(), false);
    {
      // Create state.
      QMap<QPair<unsigned int, unsigned int>, node> state;
      node& n1(state[qMakePair(42u, 24u)]);
      n1.host_id = 42;
      n1.service_id = 24;
      n1.current_state = 0;
      n1.start_time = 0;
      node& n2(state[qMakePair(56u, 13u)]);
      n2.host_id = 56;
      n2.service_id = 13;
      n2.current_state = 0;
      n2.start_time = 0;
      node& n3(state[qMakePair(90u, 0u)]);
      n3.host_id = 90;
      n3.service_id = 0;
      n3.current_state = 0;
      n3.start_time = 0;
      n2.add_dependency(&n1);
      n3.add_dependency(&n1);

      // Create correlator and apply state.
      c.set_state(state);
    }

    // Send node status.
    {  // #1
      std::shared_ptr<neb::service_status> ss(new neb::service_status);
      ss->host_id = 56;
      ss->service_id = 13;
      ss->state_type = 1;
      ss->last_hard_state = 2;
      ss->last_hard_state_change = 123456789;
      c.write(ss);
    }
    {  // #2
      std::shared_ptr<neb::host_status> hs(new neb::host_status);
      hs->host_id = 90;
      hs->state_type = 1;
      hs->last_hard_state = 1;
      hs->last_hard_state_change = 123456790;
      c.write(hs);
    }
    {  // #3
      std::shared_ptr<neb::service_status> ss(new neb::service_status);
      ss->host_id = 42;
      ss->service_id = 24;
      ss->state_type = 1;
      ss->last_hard_state = 2;
      ss->last_hard_state_change = 123456791;
      c.write(ss);
    }
    {  // #4
      std::shared_ptr<neb::service_status> ss(new neb::service_status);
      ss->host_id = 56;
      ss->service_id = 13;
      ss->state_type = 1;
      ss->last_hard_state = 1;
      ss->last_hard_state_change = 123456792;
      c.write(ss);
    }
    {  // #5
      std::shared_ptr<neb::host_status> hs(new neb::host_status);
      hs->host_id = 90;
      hs->state_type = 1;
      hs->last_hard_state = 2;
      hs->last_hard_state_change = 123456793;
      c.write(hs);
    }
    {  // #6
      std::shared_ptr<neb::service_status> ss(new neb::service_status);
      ss->host_id = 42;
      ss->service_id = 24;
      ss->state_type = 1;
      ss->last_hard_state = 0;
      ss->last_hard_state_change = 123456794;
      c.write(ss);
    }

    // Check correlation content.
    multiplexing::engine::instance().stop();
    t.finalize();
    QList<std::shared_ptr<io::data> > content;
    // #1
    add_state(content, -1, 0, 123456789, 56, false, 13, 0);
    add_state(content, -1, 2, -1, 56, false, 13, 123456789);
    add_issue(content, -1, -1, 56, 13, 123456789);
    // #2
    add_state(content, -1, 0, 123456790, 90, false, 0, 0);
    add_state(content, -1, 1, -1, 90, false, 0, 123456790);
    add_issue(content, -1, -1, 90, 0, 123456790);
    // #3
    add_state(content, -1, 0, 123456791, 42, false, 24, 0);
    add_state(content, -1, 2, -1, 42, false, 24, 123456791);
    add_issue(content, -1, -1, 42, 24, 123456791);
    add_issue_parent(content, 56, 13, 123456789, -1, 42, 24, 123456791,
                     123456791);
    add_issue_parent(content, 90, 0, 123456790, -1, 42, 24, 123456791,
                     123456791);
    // #4
    add_state(content, -1, 2, 123456792, 56, false, 13, 123456789);
    add_state(content, -1, 1, -1, 56, false, 13, 123456792);
    // #5
    add_state(content, -1, 1, 123456793, 90, false, 0, 123456790);
    add_state(content, -1, 2, -1, 90, false, 0, 123456793);
    // #6
    add_state(content, -1, 2, 123456794, 42, false, 24, 123456791);
    add_state(content, -1, 0, -1, 42, false, 24, 123456794);
    add_issue_parent(content, 56, 13, 123456789, 123456794, 42, 24, 123456791,
                     123456791);
    add_issue_parent(content, 90, 0, 123456790, 123456794, 42, 24, 123456791,
                     123456791);
    add_issue(content, -1, 123456794, 42, 24, 123456791);

    // Check.
    check_content(t, content);

    // Success.
    retval = EXIT_SUCCESS;
  } catch (std::exception const& e) {
    std::cout << e.what() << std::endl;
  } catch (...) {
    std::cout << "unknown exception" << std::endl;
  }

  // Cleanup.
  config::applier::deinit();

  return (retval);
}
