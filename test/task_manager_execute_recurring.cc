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
 *  Check the task manager execute recurring.
 *
 *  @return 0 on success.
 */
int main() {
  try {
    {
      task_manager tm;

      task_test* t1(new task_test);
      tm.add(t1, timestamp(), 1, true, true);

      if (tm.execute(timestamp::now()) != 1)
        throw (basic_error() << "execute one task failed");
      if (tm.execute(timestamp::now()) != 1)
        throw (basic_error() << "execute one task failed");
      if (tm.execute(timestamp::now()) != 1)
        throw (basic_error() << "execute one task failed");
    }

    {
      task_manager tm;

      task_test* t2(new task_test);
      tm.add(t2, timestamp(), 1, false, false);
      tm.add(t2, timestamp(), 1, false, false);
      tm.add(t2, timestamp(), 1, false, false);
      tm.add(t2, timestamp(), 1, false, false);

      if (tm.execute(timestamp::now()) != 4)
        throw (basic_error() << "execute four task failed");
      if (tm.execute(timestamp::now()) != 4)
        throw (basic_error() << "execute four task failed");
      if (tm.execute(timestamp::now()) != 4)
        throw (basic_error() << "execute four task failed");
      delete t2;
    }

    {
      task_manager tm;

      timestamp future(timestamp::now());
      future.add_second(42);

      task_test* t2(new task_test);
      tm.add(t2, future, 0, false, false);
      if (tm.execute(timestamp::now()))
        throw (basic_error() << "execute future task failed");
      delete t2;
    }
  }
  catch (std::exception const& e) {
    std::cerr << "error: " << e.what() << std::endl;
    return (1);
  }
  return (0);
}
