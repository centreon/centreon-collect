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
#include <map>
#include "com/centreon/exceptions/basic.hh"
#include "com/centreon/concurrency/thread_pool.hh"

using namespace com::centreon::concurrency;

static std::map<void*, bool> res;

/**
 *  @class task
 *  @brief litle implementation of runnable to test the thread pool.
 */
class  task : public runnable {
public:
       task(bool auto_delete) {
         set_auto_delete(auto_delete);
         res[this] = false;
       }
       ~task() throw () {}
  void run() { res[this] = true; }
};

/**
 *  Check with null pointer argument.
 *
 *  @return True on success, otherwise false.
 */
static bool null_pointer() {
  try {
    thread_pool pool;
    pool.start(NULL);
  }
  catch (std::exception const& e) {
    (void)e;
    return (true);
  }
  return (false);
}

/**
 *  Check the thread pool start.
 *
 *  @return 0 on success.
 */
int main() {
  try {
    if (!null_pointer())
      throw (basic_error() << "try to start runnable with " \
             "null pointer");

    task* t1 = new task(true);
    task* t2 = new task(false);
    thread_pool pool(1);
    pool.start(t1);
    pool.start(t2);
    pool.wait_for_done();
    if (!res[t1] || !res[t2])
      throw (basic_error() << "tasks didn't run");
    delete t2;
  }
  catch (std::exception const& e) {
    std::cerr << "error: " << e.what() << std::endl;
    return (1);
  }
  return (0);
}
