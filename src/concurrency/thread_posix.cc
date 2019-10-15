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

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <pthread.h>
#if defined(__FreeBSD__)
#include <pthread_np.h>
#elif defined(__NetBSD__)
#include <signal.h>
#include <sys/time.h>
#endif  // BSD flavor.
#include "com/centreon/exceptions/basic.hh"
#include "com/centreon/concurrency/thread_posix.hh"

using namespace com::centreon::concurrency;

/**************************************
*                                     *
*           Public Methods            *
*                                     *
**************************************/

/**
 *  Default constructor.
 */
thread::thread() : _initialized(false) {}

/**
 *  Destructor.
 */
thread::~thread() throw() {}

/**
 *  Execute the running method in the new thread.
 */
void thread::exec() {
  std::lock_guard<std::mutex> lock(_mtx);
  if (_initialized)
    throw(basic_error() << "execute thread failed: already running");
  int ret(pthread_create(&_th, NULL, &_execute, this));
  if (ret)
    throw(basic_error() << "failed to create thread: " << strerror(ret));
  _initialized = true;
}

/**
 *  Get the current thread id.
 *
 *  @return The current thread id.
 */
thread_id thread::get_current_id() throw() { return (pthread_self()); }

/**
 *  Makes the calling thread sleep untils timeout.
 *  @remark This function is static.
 *
 *  @param[in] msecs  Time to sleep in milliseconds.
 */
void thread::msleep(unsigned long msecs) {
  timespec ts;
  memset(&ts, 0, sizeof(ts));
  ts.tv_sec = msecs / 1000;
  ts.tv_nsec = (msecs % 1000) * 1000000l;
  nanosleep(&ts, NULL);
  return;
}

/**
 *  Makes the calling thread sleep untils timeout.
 *  @remark This function is static.
 *
 *  @param[in] nsecs  Time to sleep in nanoseconds.
 */
void thread::nsleep(unsigned long nsecs) {
  timespec ts;
  memset(&ts, 0, sizeof(ts));
  ts.tv_sec = nsecs / 1000000000l;
  ts.tv_nsec = nsecs % 1000000000l;
  nanosleep(&ts, NULL);
  return;
}

/**
 *  Makes the calling thread sleep untils timeout.
 *  @remark This function is static.
 *
 *  @param[in] secs  Time to sleep in seconds.
 */
void thread::sleep(unsigned long secs) {
  timespec ts;
  memset(&ts, 0, sizeof(ts));
  ts.tv_sec = secs;
  ts.tv_nsec = 0;
  nanosleep(&ts, NULL);
  return;
}

/**
 *  Makes the calling thread sleep untils timeout.
 *  @remark This function is static.
 *
 *  @param[in] usecs  Time to sleep in micoseconds.
 */
void thread::usleep(unsigned long usecs) {
  timespec ts;
  memset(&ts, 0, sizeof(ts));
  ts.tv_sec = usecs / 1000000l;
  ts.tv_nsec = (usecs % 1000000l) * 1000l;
  nanosleep(&ts, NULL);
  return;
}

/**
 *  Wait the end of the thread.
 */
void thread::wait() {
  std::lock_guard<std::mutex> lock(_mtx);

  // Wait the end of the thread.
  if (_initialized) {
    int ret(pthread_join(_th, NULL));
    if (ret && ret != ESRCH)
      throw(basic_error() << "failure while waiting thread: " << strerror(ret));
    _initialized = false;
  }
  return;
}

/**
 *  This is an overload of wait.
 *
 *  @param[in] timeout  Define the timeout to wait the end of
 *                      the thread.
 *
 *  @return true if the thread end before timeout, otherwise false.
 */
bool thread::wait(unsigned long timeout) {
  std::lock_guard<std::mutex> lock(_mtx);
  if (_initialized) {
#if defined(__NetBSD__) || defined(__OpenBSD__)
    // Implementation based on pthread_kill and usleep.

    // Get the current time.
    timeval now;
    gettimeofday(&now, NULL);
    timeval limit;
    memcpy(&limit, &now, sizeof(limit));

    // Add timeout.
    limit.tv_sec += timeout / 1000;
    timeout %= 1000;
    limit.tv_usec += timeout * 1000;
    if (limit.tv_usec > 1000000) {
      limit.tv_usec -= 1000000;
      ++limit.tv_sec;
    }

    // Wait for the end of thread or timeout.
    bool running(true);
    while (running && ((now.tv_sec * 1000000ull + now.tv_usec) <
                       (limit.tv_sec * 1000000ull + limit.tv_usec))) {
      usleep(10000);
      int ret(pthread_kill(_th, 0));
      if (ret == ESRCH)
        running = false;
      else
        throw(
            basic_error() << "failure while waiting thread: " << strerror(ret));
      gettimeofday(&now, NULL);
    }

    // Join thread.
    if (!running) {
      int ret(pthread_join(_th, NULL));
      if (ret)
        throw(
            basic_error() << "failure while waiting thread: " << strerror(ret));
      _initialized = false;
    }
    return (!running);
#else   // Other Unix systems.
    // Implementation based on pthread_timedjoin_np.

    // Get the current time.
    timespec ts;
    if (clock_gettime(CLOCK_REALTIME, &ts))
      throw(
          basic_error() << "failure while waiting thread: " << strerror(errno));

    // Add timeout.
    ts.tv_sec += timeout / 1000;
    timeout %= 1000;
    ts.tv_nsec += timeout * 1000000l;
    if (ts.tv_nsec > 1000000000l) {
      ts.tv_nsec -= 1000000000l;
      ++ts.tv_sec;
    }

    // Wait the end of the thread or timeout.
    int ret(pthread_timedjoin_np(_th, NULL, &ts));
    if (!ret || ret == ESRCH) {
      _initialized = false;
      return (true);
    }
    if (ret == ETIMEDOUT)
      return (false);
    throw(basic_error() << "failure while waiting thread: " << strerror(ret));
#endif  // Unix flavor.
  }
  return (true);
}

/**
 *  Causes the calling thread to relinquish the CPU.
 *  @remark This function is static.
 */
void thread::yield() throw() { sched_yield(); }

/**************************************
*                                     *
*           Private Methods           *
*                                     *
**************************************/

/**
 *  The thread start routine.
 *  @remark This function is static.
 *
 *  @param[in] data  This thread object.
 *
 *  @return always return zero.
 */
void* thread::_execute(void* data) {
  thread* self(static_cast<thread*>(data));
  if (self)
    self->_run();
  return (0);
}
