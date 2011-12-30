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

#include <stdlib.h>
#include <assert.h>
#if defined(_WIN32)
#  include <windows.h> // for GetSystemInfo
#elif defined(__linux__)
#  include <unistd.h> // for sysconf()
#endif // Unix flavor.
#include "com/centreon/exceptions/basic.hh"
#include "com/centreon/concurrency/locker.hh"
#include "com/centreon/concurrency/thread_pool.hh"

using namespace com::centreon::concurrency;

/**
 *  Default constructor.
 *
 *  @param[in] max_thread_count  The number of threads into the thread
 *                               pool.
 */
thread_pool::thread_pool(unsigned int max_thread_count)
  : _current_task_running(0),
    _quit(false),
    _max_thread_count(0) {
  set_max_thread_count(max_thread_count);
}

/**
 *  Default destructor.
 */
thread_pool::~thread_pool() throw () {
  {
    locker lock(&_mtx_thread);
    _quit = true;
    _cnd_thread.wake_all();
  }
  locker lock(&_mtx_pool);
  for (std::list<internal_thread*>::const_iterator
         it(_pool.begin()), end(_pool.end());
       it != end;
       ++it)
    delete *it;
}

/**
 *  Get the number of current task running.
 *
 *  @return The number of current task running.
 */
unsigned int thread_pool::get_current_task_running() const throw () {
  return (_current_task_running);
}


/**
 *  Get the number of threads into the thread pool.
 *
 *  @return The max thread count.
 */
unsigned int thread_pool::get_max_thread_count() const throw () {
  locker lock(&_mtx_pool);
  return (_max_thread_count);
}

/**
 *  Set the number of threads into the thread pool.
 *
 *  @param[in] max  The max thread count.
 */
void thread_pool::set_max_thread_count(unsigned int max) {
  locker lock(&_mtx_pool);

  // Find ideal thread count.
  if (!max) {
#if defined(_WIN32)
    SYSTEMINFO sysinfo;
    GetSystemInfo(&sysinfo);
    max = sysinfo.dwNumberOfProcessors;
#elif defined(__linux__)
    long ncpus(sysconf(_SC_NPROCESSORS_ONLN));
    if (ncpus <= 0)
      max = 1;
    else
      max = ncpus;
#elif defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__)
    size_t len(sizeof(max));
    int mib[2];
    mib[0] = CTL_HW;
    mib[1] = HW_NCPU;
    if (sysctl(mib, &max, &len, NULL, 0))
      max = 1;
#else
    max = 1;
#endif // UNIX flavor.    
  }

  if (_max_thread_count < max)
    for (unsigned int i(0), nb_thread(max - _max_thread_count);
         i < nb_thread;
         ++i) {
      internal_thread* th(new internal_thread(this));
      _pool.push_back(th);
      th->exec();
    }
  else if (_max_thread_count > max) {
    for (unsigned int i(0), nb_thread(_max_thread_count - max);
         i < nb_thread;
         ++i) {
      internal_thread* th(_pool.front());
      _pool.pop_front();
      th->quit();
      delete th;
    }
  }
  _max_thread_count = max;
}

/**
 *  Reserve a thread and uses it to run a runnable.
 *
 *  @param[in] r  The task to run.
 */
void thread_pool::start(runnable* r) {
  if (!r)
    throw (basic_error() << "impossible to start a new runnable:" \
           "invalid argument (null pointer)");

  locker lock(&_mtx_thread);
  _tasks.push_back(r);
  _cnd_thread.wake_one();
}

/**
 *  Waits for each runnable are finish.
 */
void thread_pool::wait_for_done() {
  locker lock(&_mtx_thread);
  while (!_tasks.empty() || _current_task_running)
    _cnd_pool.wait(&_mtx_thread);
}

/**
 *  Default copy constructor.
 *
 *  @param[in] right  The object to copy.
 */
thread_pool::thread_pool(thread_pool const& right) {
  _internal_copy(right);
}

/**
 *  Default copy operator.
 *
 *  @param[in] right  The object to copy.
 *
 *  @return This object.
 */
thread_pool& thread_pool::operator=(thread_pool const& right) {
  return (_internal_copy(right));
}

/**
 *  Internal copy.
 *
 *  @param[in] right  The object to copy.
 *
 *  @return This object.
 */
thread_pool& thread_pool::_internal_copy(thread_pool const& right) {
  (void)right;
  assert(!"impossible to copy thread_pool");
  abort();
  return (*this);
}

/**
 *  Default constructor.
 *
 *  @param[in] th_pool  The thread pool which is attached to this
 *                      thread.
 */
thread_pool::internal_thread::internal_thread(thread_pool* th_pool)
  : thread(),
    _quit(false),
    _th_pool(th_pool) {

}

/**
 *  Default destructor.
 */
thread_pool::internal_thread::~internal_thread() throw () {
  wait();
}

/**
 *  Ask the thread to quit.
 */
void thread_pool::internal_thread::quit() {
  locker lock(&_th_pool->_mtx_thread);
  _quit = true;
  _th_pool->_cnd_thread.wake_all();
}

/**
 *  Default copy constructor.
 *
 *  @param[in] right  The object to copy.
 */
thread_pool::internal_thread::internal_thread(internal_thread const& right)
  : thread() {
  _internal_copy(right);
}

/**
 *  Default copy operator.
 *
 *  @param[in] right  The object to copy.
 *
 *  @return This object.
 */
thread_pool::internal_thread& thread_pool::internal_thread::operator=(internal_thread const& right) {
  return (_internal_copy(right));
}

/**
 *  Internal copy.
 *
 *  @param[in] right  The object to copy.
 *
 *  @return This object.
 */
thread_pool::internal_thread& thread_pool::internal_thread::_internal_copy(internal_thread const& right) {
  (void)right;
  assert(!"thread_pool::internal_thread is not copyable");
  abort();
  return (*this);
}

/**
 *  Internal running method to run the runnable class.
 */
void thread_pool::internal_thread::_run() {
  locker lock(&_th_pool->_mtx_thread);
  while (true) {
    while (!_th_pool->_tasks.empty()) {
      runnable* task(_th_pool->_tasks.front());
      _th_pool->_tasks.pop_front();
      ++_th_pool->_current_task_running;
      lock.unlock();
      task->run();
      if (task->get_auto_delete())
        delete task;
      lock.relock();
      --_th_pool->_current_task_running;
      _th_pool->_cnd_pool.wake_one();
    }
    if (_th_pool->_quit || _quit)
      break;
    _th_pool->_cnd_thread.wait(&_th_pool->_mtx_thread);
  }
}
