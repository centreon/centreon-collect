/*
** Copyright 2013 Centreon
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

#include <cmath>
#include <cstdlib>
#include <QList>
#include "com/centreon/broker/config/applier/init.hh"
#include "com/centreon/broker/exceptions/msg.hh"
#include "com/centreon/broker/storage/parser.hh"
#include "com/centreon/broker/storage/perfdata.hh"

using namespace com::centreon::broker;
using namespace com::centreon::broker::storage;

/**
 *  Check that incorrect perfdata generate an error in the parser.
 *
 *  @return EXIT_SUCCESS on success.
 */
int main() {
  // Initialization.
  config::applier::init();

  // Objects.
  QList<perfdata> list;
  parser p;

  bool success;
  try {
    // Parse perfdata string.
    p.parse_perfdata(
        "metric1= 10 metric2=42",
        list);

    // No success.
    success = false;
  }
  catch (exceptions::msg const& e) {
    (void)e;
    success = true;
  }

  // Cleanup.
  config::applier::deinit();

  return (success ? EXIT_SUCCESS : EXIT_FAILURE);
}
