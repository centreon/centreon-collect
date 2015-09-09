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
#include "com/centreon/concurrency/thread_pool.hh"

using namespace com::centreon::concurrency;

/**
 *  Check the thread pool max thread count.
 *
 *  @return 0 on success.
 */
int main() {
  try {
    thread_pool pool(4);
    if (pool.get_max_thread_count() != 4)
      throw (basic_error() << "get_max_thread_count failed");
    pool.set_max_thread_count(1);
    if (pool.get_max_thread_count() != 1)
      throw (basic_error() << "set_max_thread_count failed");
    pool.set_max_thread_count(0);
    if (pool.get_max_thread_count() <= 0)
      throw (basic_error() << "set_max_thread_count failed");
  }
  catch (std::exception const& e) {
    std::cerr << "error: " << e.what() << std::endl;
    return (1);
  }
  return (0);
}
