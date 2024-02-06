/**
 * Copyright 2020 Centreon (https://www.centreon.com/)
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * For more information : contact@centreon.com
 *
 */
#include "com/centreon/task_manager.hh"
#include <gtest/gtest.h>
#include <iostream>
#include "com/centreon/exceptions/basic.hh"
// Global task manager.

using namespace com::centreon;

task_manager tm;

class task_test : public task {
 public:
  task_test() : task() {}
  ~task_test() noexcept {}
  void run() {}
};

class task_test2 : public task {
 public:
  task_test2() : task() {}
  ~task_test2() noexcept {}
  void run() { tm.remove(this); }
};

TEST(ClibTaskManager, Add) {
  task_manager tm;

  task_test* t1 = new task_test;
  tm.add(t1, timestamp::now(), true, true);
  ASSERT_TRUE(tm.next_execution_time().to_useconds());

  task_test* t2 = new task_test;
  tm.add(t2, timestamp::now(), false, false);
  delete t2;
}

TEST(ClibTaskManager, AddRecurring) {
  task_manager tm;

  task_test* t1(new task_test);
  tm.add(t1, timestamp::now(), 10, true, true);
  ASSERT_TRUE(tm.next_execution_time().to_useconds());

  task_test* t2(new task_test);
  tm.add(t2, timestamp::now(), 10, false, false);
  delete t2;
}

TEST(ClibTaskManager, Execute) {
  task_manager tm;

  ASSERT_FALSE(tm.execute(timestamp::now()));

  task_test* t1(new task_test);
  tm.add(t1, timestamp::now(), true, true);
  ASSERT_TRUE(tm.next_execution_time().to_useconds());

  ASSERT_EQ(tm.execute(timestamp::now()), 1u);

  task_test* t2(new task_test);
  tm.add(t2, timestamp(), false, false);
  tm.add(t2, timestamp(), false, false);
  tm.add(t2, timestamp(), false, false);
  tm.add(t2, timestamp(), false, false);

  ASSERT_EQ(tm.execute(timestamp::now()), 4u);

  timestamp future(timestamp::now());
  future.add_seconds(42);
  tm.add(t2, future, false, false);
  ASSERT_FALSE(tm.execute(timestamp::now()));
  delete t2;
}

TEST(ClibTaskManager, ExecuteRecurring) {
  {
    task_manager tm;

    task_test* t1(new task_test);
    tm.add(t1, timestamp(), 1, true, true);

    ASSERT_EQ(tm.execute(timestamp::now()), 1u);
    ASSERT_EQ(tm.execute(timestamp::now()), 1u);
    ASSERT_EQ(tm.execute(timestamp::now()), 1u);
  }

  {
    task_manager tm;

    task_test* t2(new task_test);
    tm.add(t2, timestamp(), 1, false, false);
    tm.add(t2, timestamp(), 1, false, false);
    tm.add(t2, timestamp(), 1, false, false);
    tm.add(t2, timestamp(), 1, false, false);

    ASSERT_EQ(tm.execute(timestamp::now()), 4u);
    ASSERT_EQ(tm.execute(timestamp::now()), 4u);
    ASSERT_EQ(tm.execute(timestamp::now()), 4u);
    delete t2;
  }

  {
    task_manager tm;

    timestamp future(timestamp::now());
    future.add_seconds(42);

    task_test* t2(new task_test);
    tm.add(t2, future, 0, false, false);
    ASSERT_FALSE(tm.execute(timestamp::now()));
    delete t2;
  }
}

TEST(ClibTaskManager, NextExecutionTime) {
  task_manager tm;
  timestamp now(timestamp::now());
  timestamp max_time(timestamp::max_time());

  ASSERT_EQ(tm.next_execution_time(), max_time);

  task_test* t1(new task_test);
  tm.add(t1, now, true, true);
  ASSERT_EQ(tm.next_execution_time(), now);
}

TEST(ClibTaskManager, RemoveById) {
  task_manager tm;

  task_test* t1(new task_test);
  unsigned long id1(tm.add(t1, timestamp::now(), true, true));

  ASSERT_FALSE(tm.remove(42));

  ASSERT_TRUE(tm.remove(id1));

  task_test* t2(new task_test);
  unsigned long id2(tm.add(t2, timestamp::now(), false, false));
  ASSERT_TRUE(tm.remove(id2));
  delete t2;

  ASSERT_FALSE(tm.remove(42));

  ASSERT_EQ(tm.next_execution_time(), timestamp::max_time());
}

TEST(ClibTaskManager, RemoveByTask) {
  task_manager tm;

  task_test* t1(new task_test);
  tm.add(t1, timestamp::now(), true, true);

  task_test none;
  ASSERT_FALSE(tm.remove(&none));

  ASSERT_EQ(tm.remove(t1), 1u);

  task_test* t2(new task_test);
  tm.add(t2, timestamp::now(), false, false);
  tm.add(t2, timestamp::now(), false, false);
  tm.add(t2, timestamp::now(), false, false);
  tm.add(t2, timestamp::now(), false, false);
  ASSERT_EQ(tm.remove(t2), 4u);
  delete t2;

  ASSERT_FALSE(tm.remove(reinterpret_cast<task*>(0x4242)));

  ASSERT_EQ(tm.next_execution_time(), timestamp::max_time());
}

TEST(ClibTaskManager, RemoveSelf) {
  task_test* t1(new task_test);
  tm.add(t1, timestamp::now(), true, true);
  tm.execute();

  task_test* t2(new task_test);
  tm.add(t2, timestamp::now(), false, false);
  tm.add(t2, timestamp::now(), false, false);
  tm.add(t2, timestamp::now(), false, false);
  tm.add(t2, timestamp::now(), false, false);
  tm.execute();
  delete t2;
}
