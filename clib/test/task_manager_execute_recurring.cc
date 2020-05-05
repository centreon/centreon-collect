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
#include "com/centreon/exceptions/basic.hh"
#include "com/centreon/task_manager.hh"

using namespace com::centreon;

/**
 *  @class task_test
 *  @brief litle implementation of task to test task manager.
 */
class task_test : public task {
 public:
  task_test() : task() {}
  ~task_test() noexcept {}
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
        throw basic_error() << "execute one task failed";
      if (tm.execute(timestamp::now()) != 1)
        throw basic_error() << "execute one task failed";
      if (tm.execute(timestamp::now()) != 1)
        throw basic_error() << "execute one task failed";
    }

    {
      task_manager tm;

      task_test* t2(new task_test);
      tm.add(t2, timestamp(), 1, false, false);
      tm.add(t2, timestamp(), 1, false, false);
      tm.add(t2, timestamp(), 1, false, false);
      tm.add(t2, timestamp(), 1, false, false);

      if (tm.execute(timestamp::now()) != 4)
        throw basic_error() << "execute four task failed";
      if (tm.execute(timestamp::now()) != 4)
        throw basic_error() << "execute four task failed";
      if (tm.execute(timestamp::now()) != 4)
        throw basic_error() << "execute four task failed";
      delete t2;
    }

    {
      task_manager tm;

      timestamp future(timestamp::now());
      future.add_seconds(42);

      task_test* t2(new task_test);
      tm.add(t2, future, 0, false, false);
      if (tm.execute(timestamp::now()))
        throw(basic_error() << "execute future task failed");
      delete t2;
    }
  } catch (std::exception const& e) {
    std::cerr << "error: " << e.what() << std::endl;
    return 1;
  }
  return 0;
}
