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

#include <sstream>
#include "com/centreon/task_manager.hh"
#include <algorithm>
#include <unistd.h>
#include <cassert>

using namespace com::centreon;

/**
 *  Constructor.
 *
 *  @param[in] max_thread_count  The number of threads into the thread
 *                               pool.
 */
task_manager::task_manager(uint32_t max_thread_count)
    : _current_id{0}, _exit{false} {
  if (max_thread_count == 0) {
    int32_t ncpus = sysconf(_SC_NPROCESSORS_ONLN);
    max_thread_count = (ncpus <= 0) ? 1 : ncpus;
  }

  for (uint32_t i = 0; i < max_thread_count; ++i)
    _workers.emplace_back([this] {
      while (true) {
        internal_task* t;
        {
          std::unique_lock<std::mutex> lock(_queue_m);
          _queue_cv.wait(lock, [this] { return _exit || !_queue.empty(); });
          if (_exit && _queue.empty())
            return;

          t = _queue.front();
          _queue.pop_front();
        }
        t->tsk->run();
        if (t->interval == 0) // auto_delete
          delete t;
        _queue_cv.notify_one();
      }
    });
}

task_manager::~task_manager() {
  {
    std::lock_guard<std::mutex> lock(_queue_m);
    _exit = true;
    _queue_cv.notify_all();
  }
  for (auto& w : _workers)
    w.join();
}

/**
 *  This method is an overload to add.
 *  @remark This method is thread safe.
 *
 *  @param[in] t              The new task.
 *  @param[in] when           The time limit to execute the task.
 *  @param[in] is_runnable    If the task should run simultaneously.
 *  @param[in] should_delete  If the task should delete after running.
 *
 *  @return The id of the new task in the task manager.
 */
uint64_t task_manager::add(task* t,
                       timestamp const& when,
                       bool is_runnable,
                       bool should_delete) {
  system("echo 'task_manager::add lock _tasks_m' >> /tmp/titi");
  std::lock_guard<std::mutex> lock(_tasks_m);

  // FIXME DBR
  std::ostringstream oss;
  oss << "echo 'task_manager::add1...runnable"
    << is_runnable << "' >> /tmp/titi";
  system(oss.str().c_str());

  internal_task* itask =
      new internal_task(t, ++_current_id, 0, is_runnable, should_delete);
  _tasks.insert({when, itask});
  system("echo 'task_manager::add unlock _tasks_m' >> /tmp/titi");
  return _current_id;
}

/**
 *  This method is an overload to add.
 *  @remark This method is thread safe.
 *
 *  @param[in] t              The new task.
 *  @param[in] when           The time limit to execute the task.
 *  @param[in] interval       Define the recurrency of the task.
 *  @param[in] is_runnable    If the task should run simultaneously.
 *  @param[in] should_delete  If the task should delete after running.
 *
 *  @return The id of the new task in the task manager.
 */
uint64_t task_manager::add(task* t,
                       timestamp const& when,
                       uint32_t interval,
                       bool is_runnable,
                       bool should_delete) {
  system("echo 'task_manager::add2 lock _tasks_m' >> /tmp/titi");
  std::lock_guard<std::mutex> lock(_tasks_m);

  // FIXME DBR
  std::ostringstream oss;
  oss << "echo 'task_manager::add2...runnable"
    << is_runnable << "' >> /tmp/titi";
  system(oss.str().c_str());

  internal_task* itask = new internal_task(t, ++_current_id, interval, is_runnable, should_delete);
  _tasks.insert({when, itask});
  return _current_id;
}

/**
 *  Get the next execution time.
 *  @remark This method is thread safe.
 *
 *  @return The next time to execute task or null timestamp if no
 *          task need to be run.
 */
timestamp task_manager::next_execution_time() const {
  system("echo 'task_manager::next_execution_time lock _tasks_m' >> /tmp/titi");
  std::lock_guard<std::mutex> lock(_tasks_m);
  auto front = _tasks.begin();
  return (front == _tasks.end()) ? timestamp::max_time() : front->first;
}

/**
 *  Remove task.
 *  @remark This method is thread safe.
 *
 *  @param[in] t  The specific task.
 *
 *  @return The number of removed tasks.
 */
uint32_t task_manager::remove(task* t) {
  if (!t)
    return 0;

  // Lock the task manager.
  system("echo 'task_manager::remove lock _tasks_m' >> /tmp/titi");
  std::lock_guard<std::mutex> lock(_tasks_m);

  uint32_t retval = 0;
  for (auto it = _tasks.begin(), end = _tasks.end(); it != end; ) {
    if (it->second->tsk == t) {
      if (it->second->interval == 0)  // auto_delete
        delete it->second;
      it = _tasks.erase(it);
      ++retval;
    }
    else
      ++it;
  }
  return retval;
}

/**
 *  Remove task.
 *  @remark This method is thread safe.
 *
 *  @param[in] id The task id to remove.
 *
 *  @return A boolean telling if the task has been removed.
 */
bool task_manager::remove(uint64_t id) {
  // Lock the task manager.
  system("echo 'task_manager::remove2 lock _tasks_m' >> /tmp/titi");
  std::lock_guard<std::mutex> lock(_tasks_m);

  uint32_t retval = 0;
  for (auto it = _tasks.begin(), end = _tasks.end(); it != end; ++it) {
    if (it->second->id == id) {
      if (it->second->interval == 0)  // auto_delete
        delete it->second;
      _tasks.erase(it);
      return true;
    }
  }
  return false;
}

/**
 *  Execute all the task to need run before the time limit.
 *  @remark This method is thread safe.
 *
 *  @param[in] now  The time limit to execute tasks.
 *
 *  @return The number of task to be execute.
 */
uint32_t task_manager::execute(timestamp const& now) {
  system("echo 'execute1...' >> /tmp/titi");
  std::deque<std::pair<timestamp, internal_task*>> recurring;
  uint32_t retval = 0;
  system("echo 'task_manager::execute lock _tasks_m' >> /tmp/titi");
  std::unique_lock<std::mutex> lock(_tasks_m);
  auto it = _tasks.begin();
  while (it != _tasks.end() && it->first <= now) {
    system("echo 'execute1 while' >> /tmp/titi");
    // Get internal task.
    internal_task* itask = it->second;

    system("echo 'execute1 erase' >> /tmp/titi");
    // Remove entry
    _tasks.erase(it);

    if (itask->interval) {
      system("echo 'execute1 interval' >> /tmp/titi");
      timestamp new_time(now);
      new_time.add_useconds(itask->interval);
      recurring.emplace_back(std::make_pair(new_time, itask));
    }

    system("echo 'execute1 unlock' >> /tmp/titi");
    lock.unlock();

    if (itask->is_runnable) {
      system("echo 'execute1 enqueue task' >> /tmp/titi");
      _enqueue(itask);
    }
    else {
      system("echo 'execute1 wait...' >> /tmp/titi");
      /* This task needs to be run in the main thread without any concurrency */
      _wait_for_queue_empty();
      itask->tsk->run();
      if (itask->interval == 0) // auto_delete
        delete itask;
    }
    ++retval;

    /* Reset iterator */
    lock.lock();
    it = _tasks.begin();
  }

  system("echo 'execute2 recurring tasks inserted...' >> /tmp/titi");
  /* Update the task table with recurring tasks. */
  for (auto& t : recurring) {
    _tasks.insert(t);
  }
  system("echo 'execute3...' >> /tmp/titi");

  lock.unlock();
  /* Wait for task ending. */
  _wait_for_queue_empty();
  system("echo 'execute4...' >> /tmp/titi");
  return retval;
}

/**
 *  Add a task to the thread pool
 *
 * @param t The task to add
 */
void task_manager::_enqueue(internal_task* t) {
  std::lock_guard<std::mutex> lock(_queue_m);
  _queue.push_back(t);
  _queue_cv.notify_all();
}

void task_manager::_wait_for_queue_empty() const {
  system("echo 'task_manager::wait_for_queue_empty lock _tasks_m' >> /tmp/titi");
  std::unique_lock<std::mutex> lock(_queue_m);
  _queue_cv.wait(lock, [this] { return _queue.empty(); });
  system("echo 'task_manager::wait_for_queue_empty EMPTY' >> /tmp/titi");
}
