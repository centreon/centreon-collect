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

#include <exception>
#include <iostream>
#include "com/centreon/exceptions/basic.hh"
#include "com/centreon/concurrency/mutex.hh"
#include "com/centreon/concurrency/locker.hh"

using namespace com::centreon::concurrency;

/**
 *  Check the locker constructor.
 *
 *  @return 0 on success.
 */
int main() {
  try {
    mutex mtx;
    locker lock1;
    locker lock2(&mtx);
    if (lock2.get_mutex() != &mtx
        || lock1.get_mutex() != NULL)
      throw (basic_error() << "constructor failed");
  }
  catch (std::exception const& e) {
    std::cerr << "error: " << e.what() << std::endl;
    return (1);
  }
  return (0);
}
