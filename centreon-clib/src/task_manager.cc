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

#include <cstdlib>
#include "com/centreon/concurrency/locker.hh"
#include "com/centreon/task_manager.hh"

using namespace com::centreon;
using namespace com::centreon::concurrency;

/**
 *  Constructor.
 *
 *  @param[in] max_thread_count  The number of threads into the thread
 *                               pool.
 */
task_manager::task_manager(unsigned int max_thread_count)
  : _current_id(0), _th_pool(max_thread_count) {}

/**
 *  Destructor.
 */
task_manager::~task_manager() throw () {
  // Wait the end of all running task.
  _th_pool.wait_for_done();

  // Lock the task manager.
  locker lock(&_mtx);

  // Delete all internal task.
  for (std::multimap<timestamp, internal_task*>::const_iterator
         it(_tasks.begin()), end(_tasks.end());
       it != end;
       ++it)
    delete it->second;
}

/**
 *  Add a new task into the task manager.
 *  @remark This method is thread safe.
 *
 *  @param[in] t              The new task.
 *  @param[in] when           The time limit to execute the task.
 *  @param[in] is_runnable    If the task should run simultaneously.
 *  @param[in] should_delete  If the task should delete after running.
 *
 *  @return The id of the new task in the task manager.
 */
unsigned long task_manager::add(
                              task* t,
                              timestamp const& when,
                              bool is_runnable,
                              bool should_delete) {
  // Lock the task manager.
  locker lock(&_mtx);

  internal_task* itask(new internal_task(
                             ++_current_id,
                             t,
                             when,
                             0,
                             is_runnable,
                             should_delete));
  _tasks.insert(std::pair<timestamp, internal_task*>(when, itask));
  return (itask->id);
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
unsigned long task_manager::add(
                              task* t,
                              timestamp const& when,
                              unsigned int interval,
                              bool is_runnable,
                              bool should_delete) {
  // Lock the task manager.
  locker lock(&_mtx);

  internal_task* itask(new internal_task(
                             ++_current_id,
                             t,
                             when,
                             interval,
                             is_runnable,
                             should_delete));
  _tasks.insert(std::pair<timestamp, internal_task*>(when, itask));
  return (itask->id);
}

/**
 *  Execute all the task to need run before the time limit.
 *  @remark This method is thread safe.
 *
 *  @param[in] now  The time limit to execute tasks.
 *
 *  @return The number of task to be execute.
 */
unsigned int task_manager::execute(timestamp const& now) {
  // Stock the new recurring task into this list for inject all
  // task at the end of the execution.
  std::list<std::pair<timestamp, internal_task*> > recurring;

  unsigned int count_execute(0);
  {
    // Lock the task manager.
    locker lock(&_mtx);

    std::multimap<timestamp, internal_task*>::iterator
      it(_tasks.begin());
    while (!_tasks.empty() && (it->first <= now)) {
      // Get internal task.
      internal_task* itask(it->second);

      // Remove entry.
      _tasks.erase(it);

      if (itask->interval) {
        // This task is recurring, push it into recurring list.
        timestamp new_time(now);
        new_time.add_useconds(itask->interval);
        recurring.push_back(
                    std::pair<timestamp, internal_task*>(
                      new_time,
                      itask));
      }

      if (itask->is_runnable) {
        // This task allow to run in the thread.
        _th_pool.start(itask);
      }
      else {
        // This task need to be run in the main thread without
        // any concurrence.
        lock.unlock();
        _th_pool.wait_for_done();
        itask->t->run();
        lock.relock();
        if (itask->get_auto_delete())
          delete itask;
      }
      ++count_execute;

      // Reset iterator.
      it = _tasks.begin();
    }

    // Update the task table with the recurring task.
    for (std::list<std::pair<timestamp, internal_task*> >::const_iterator
           it(recurring.begin()), end(recurring.end());
         it != end;
         ++it) {
      it->second->when = it->first;
      _tasks.insert(*it);
    }
  }

  // Wait task ending.
  _th_pool.wait_for_done();
  return (count_execute);
}

/**
 *  Get the next execution time.
 *  @remark This method is thread safe.
 *
 *  @return The next time to execute task or null timestamp if no
 *          task need to be run.
 */
timestamp task_manager::next_execution_time() const {
  locker lock(&_mtx);
  std::multimap<timestamp, internal_task*>::const_iterator
    lower(_tasks.begin());
  return ((lower == _tasks.end())
          ? timestamp::max_time()
          : lower->first);
}

/**
 *  Remove task.
 *  @remark This method is thread safe.
 *
 *  @param[in] t  The specific task.
 *
 *  @return The number of remove task.
 */
unsigned int task_manager::remove(task* t) {
  if (!t)
    return (0);

  // Lock the task manager.
  locker lock(&_mtx);

  unsigned int count_erase(0);
  for (std::multimap<timestamp, internal_task*>::iterator
         it(_tasks.begin()), next(it), end(_tasks.end());
       it != end;
       it = next)
    if (it->second->t != t)
      ++next;
    else {
      if (it->second->get_auto_delete())
        delete it->second;
      ++next;
      _tasks.erase(it);
      ++count_erase;
    }
  return (count_erase);
}

/**
 *  This method is an overload of remove.
 *  @remark This method is thread safe.
 *
 *  @param[in] id  The task id to remove.
 *
 *  @return True if the task was remove, otherwise false.
 */
bool task_manager::remove(unsigned long id) {
  // Lock the task manager.
  locker lock(&_mtx);

  for (std::multimap<timestamp, internal_task*>::iterator
         it(_tasks.begin()), next(it), end(_tasks.end());
       it != end;
       it = next)
    if (it->second->id != id)
      ++next;
    else {
      if (it->second->get_auto_delete())
        delete it->second;
      ++next;
      _tasks.erase(it);
      return (true);
    }
  return (false);
}

/**
 *  Default constructor.
 *
 *  @param[in] _id             The task id.
 *  @param[in] _t              The task.
 *  @param[in] _when           The time limit to execute the task.
 *  @param[in] _interval       Define the recurrency of the task.
 *  @param[in] _is_runnable    If the task should run simultaneously.
 *  @param[in] _should_delete  If the task should delete after running.
 */
task_manager::internal_task::internal_task(
                           unsigned long _id,
                           task* _t,
                           timestamp const& _when,
                           unsigned int _interval,
                           bool _is_runnable,
                           bool _should_delete)
  : runnable(),
    id(_id),
    interval(_interval),
    is_runnable(_is_runnable),
    should_delete(_should_delete),
    t(_t),
    when(_when) {
  // If the task is recurring we need to set auto delete at false, to
  // reuse this task.
  set_auto_delete(!interval);
}

/**
 *  Default copy constructor.
 *
 *  @param[in] right  The object to copy.
 */
task_manager::internal_task::internal_task(internal_task const& right)
  : runnable() {
  _internal_copy(right);
}

/**
 *  Default destructor.
 */
task_manager::internal_task::~internal_task() throw () {
  if (should_delete)
    delete t;
}

/**
 *  Default copy operator.
 *
 *  @param[in] right  The object to copy.
 *
 *  @return This object.
 */
task_manager::internal_task& task_manager::internal_task::operator=(internal_task const& right) {
  return (_internal_copy(right));
}

/**
 *  Run a task.
 */
void task_manager::internal_task::run() {
  t->run();
}

/**
 *  Internal copy.
 *
 *  @param[in] right  The object to copy.
 *
 *  @return This object.
 */
task_manager::internal_task& task_manager::internal_task::_internal_copy(internal_task const& right) {
  if (this != &right) {
    runnable::operator=(right);
    id = right.id;
    interval = right.interval;
    is_runnable = right.is_runnable;
    t = right.t;
    when = right.when;
  }
  return (*this);
}

