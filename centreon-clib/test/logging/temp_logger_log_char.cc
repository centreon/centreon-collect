/*
** Copyright 2011-2013 Centreon
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

#include <iostream>
#include <memory>
#include "com/centreon/exceptions/basic.hh"
#include "com/centreon/logging/engine.hh"
#include "com/centreon/logging/temp_logger.hh"
#include "./backend_test.hh"

using namespace com::centreon::logging;

/**
 *  Check temp logger with nothing to do.
 *
 *  @return 0 on success.
 */
int main() {
  int retval;

  engine::load();
  try {
    engine& e(engine::instance());
    std::unique_ptr<backend_test> obj(new backend_test);
    e.add(obj.get(), 3, 0);

    temp_logger(1, 0) << 'c';
    if (obj->get_nb_call() != 1)
      throw (basic_error() << "invalid number of call log");
    retval = 0;
  }
  catch (std::exception const& e) {
    std::cerr << "error: " << e.what() << std::endl;
    retval = 1;
  }
  engine::unload();
  return (retval);
}
