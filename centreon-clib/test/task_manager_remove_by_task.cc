/*
** Copyright 2011-2019 Centreon
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
      throw basic_error() << "remove invalid task";

    if (tm.remove(t1) != 1)
      throw basic_error() << "remove one task failed";

    task_test* t2(new task_test);
    tm.add(t2, timestamp::now(), false, false);
    tm.add(t2, timestamp::now(), false, false);
    tm.add(t2, timestamp::now(), false, false);
    tm.add(t2, timestamp::now(), false, false);
    if (tm.remove(t2) != 4)
      throw basic_error() << "remove four task failed";
    delete t2;

    if (tm.remove(reinterpret_cast<task*>(0x4242)))
      throw basic_error() << "remove invalid task failed";

    if (tm.next_execution_time() != timestamp::max_time())
      throw basic_error() << "invalid next_execution_time";
  } catch (std::exception const& e) {
    std::cerr << "error: " << e.what() << std::endl;
    return 1;
  }
  return 0;
}
