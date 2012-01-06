/*
** Copyright 2011-2012 Merethis
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

#include <iostream>
#include "com/centreon/exceptions/basic.hh"
#include "com/centreon/task_manager.hh"

using namespace com::centreon;

/**
 *  @class task_test
 *  @brief litle implementation of task to test task manager.
 */
class  task_test : public task {
public:
       task_test() : task() {}
       ~task_test() throw () {}
  void run() {}
};

/**
 *  Check the task manager remove by task.
 *
 *  @return 0 on success.
 */
int main() {
  try {
    task_manager tm;

    task_test* t1(new task_test);
    tm.add(t1, timestamp::now(), true, true);

    task_test none;
    if (tm.remove(&none))
      throw (basic_error() << "remove invalid task");

    if (tm.remove(t1) != 1)
      throw (basic_error() << "remove one task failed");

    task_test* t2(new task_test);
    tm.add(t2, timestamp::now(), false, false);
    tm.add(t2, timestamp::now(), false, false);
    tm.add(t2, timestamp::now(), false, false);
    tm.add(t2, timestamp::now(), false, false);
    if (tm.remove(t2) != 4)
      throw (basic_error() << "remove four task failed");
    delete t2;

    if (tm.remove(reinterpret_cast<task*>(0x4242)))
      throw (basic_error() << "remove invalid task failed");

    if (tm.next_execution_time().to_useconds())
      throw (basic_error() << "invalid next_execution_time");
  }
  catch (std::exception const& e) {
    std::cerr << "error: " << e.what() << std::endl;
    return (1);
  }
  return (0);
}
