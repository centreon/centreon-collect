/*
** Copyright 2011 Merethis
**
** This file is part of Centreon Clib.
**
** Centreon Clib is free software: you can redistribute it
** and/or modify it under the terms of the GNU Affero General Public
** License as published by the Free Software Foundation, either version
** 3 of the License, or (at your option) any later version.
**
** Centreon Clib is distributed in the hope that it will be
** useful, but WITHOUT ANY WARRANTY; without even the implied warranty
** of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
** Affero General Public License for more details.
**
** You should have received a copy of the GNU Affero General Public
** License along with Centreon Clib. If not, see
** <http://www.gnu.org/licenses/>.
*/

#include <exception>
#include <iostream>
#include <map>
#include "com/centreon/exception/basic.hh"
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
