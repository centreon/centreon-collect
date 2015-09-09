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
#if defined(_WIN32)
#  include <windows.h> // for GetSystemInfo
#elif defined(__FreeBSD__)
#  include <sys/types.h>
#  include <sys/sysctl.h>
#elif defined(__NetBSD__)
#  include <sys/sysctl.h>
#elif defined(__OpenBSD__)
#  include <sys/param.h>
#  include <sys/sysctl.h>
#endif // OS flavor.
#ifndef _WIN32
#  include <unistd.h>
#endif // POSIX.
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
    _max_thread_count(0),
#ifndef _WIN32
    _pid(getpid()),
#endif // !Windows
    _quit(false) {
  set_max_thread_count(max_thread_count);
}

/**
 *  Default destructor.
 */
thread_pool::~thread_pool() throw () {
#ifndef _WIN32
  if (getpid() == _pid) {
#endif // !Windows
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
#ifndef _WIN32
  }
#endif // !Windows
}

/**
 *  Get the number of current task running.
 *  @remark This method is thread safe.
 *
 *  @return The number of current task running.
 */
unsigned int thread_pool::get_current_task_running() const throw () {
  // Lock the thread.
  locker lock(&_mtx_thread);
  return (_current_task_running);
}


/**
 *  Get the number of threads into the thread pool.
 *  @remark This method is thread safe.
 *
 *  @return The max thread count.
 */
unsigned int thread_pool::get_max_thread_count() const throw () {
  // Lock the thread pool.
  locker lock(&_mtx_pool);
  return (_max_thread_count);
}

/**
 *  Set the number of threads into the thread pool.
 *  @remark This method is thread safe.
 *
 *  @param[in] max  The max thread count.
 */
void thread_pool::set_max_thread_count(unsigned int max) {
  // Lock the thread pool.
  locker lock(&_mtx_pool);

  // Find ideal thread count.
  if (!max) {
#if defined(_WIN32)
    SYSTEM_INFO sysinfo;
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
    if (sysctl(mib, 2, &max, &len, NULL, 0))
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
 *  @remark This method is thread safe.
 *
 *  @param[in] r  The task to run.
 */
void thread_pool::start(runnable* r) {
  if (!r)
    throw (basic_error() << "impossible to start a new runnable:" \
           "invalid argument (null pointer)");

  // Lock the thread.
  locker lock(&_mtx_thread);
  _tasks.push_back(r);
  _cnd_thread.wake_one();
}

/**
 *  Waits for each runnable are finish.
 *  @remark This method is thread safe.
 */
void thread_pool::wait_for_done() {
  // Lock the thread.
  locker lock(&_mtx_thread);
  while (!_tasks.empty() || _current_task_running)
    _cnd_pool.wait(&_mtx_thread);
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
 *  @remark This method is thread safe.
 */
void thread_pool::internal_thread::quit() {
  // Lock the thread.
  locker lock(&_th_pool->_mtx_thread);
  _quit = true;
  _th_pool->_cnd_thread.wake_all();
}

/**
 *  Internal running method to run the runnable class.
 *  @remark This method is thread safe.
 */
void thread_pool::internal_thread::_run() {
  // Lock the thread.
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
