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
#include <sys/time.h>
#include <unistd.h>
#include "com/centreon/exceptions/basic.hh"
#include "com/centreon/concurrency/semaphore_posix.hh"

using namespace com::centreon::concurrency;

/**************************************
*                                     *
*           Public Methods            *
*                                     *
**************************************/

/**
 *  Default constructor.
 *
 *  @param[in] n  Specifies the initial value for the semaphore.
 */
semaphore::semaphore(unsigned int n) {
  if (sem_init(&_sem, 0, n)) {
    char const* msg(strerror(errno));
    throw(basic_error() << "unable to create semaphore: " << msg);
  }
}

/**
 *  Destructor.
 */
semaphore::~semaphore() throw() { sem_destroy(&_sem); }

/**
 *  Acquire one ressource. If the semaphore's value is greater than zero
 *  then the function returns immediately, else the call blocks until
 *  one ressource is released.
 */
void semaphore::acquire() {
  if (sem_wait(&_sem)) {
    char const* msg(strerror(errno));
    throw(basic_error() << "unable to acquire semaphore: " << msg);
  }
  return;
}

/**
 *  This is an overload of acquire().
 *
 *  @param[in] timeout Maximum number of milliseconds to wait for
 *                     ressource availability.
 *
 *  @return True if one ressource is acquire, false if timeout.
 */
bool semaphore::acquire(unsigned long timeout) {
#if defined(_POSIX_TIMEOUTS) && (_POSIX_TIMEOUTS >= 200112L)
  // Implementation based on sem_timedwait.

  // Get the current time.
  timespec ts;
  if (clock_gettime(CLOCK_REALTIME, &ts)) {
    char const* msg(strerror(errno));
    throw(basic_error() << "unable to get time within semaphore: " << msg);
  }

  // Add the timeout.
  ts.tv_sec += timeout / 1000;
  timeout %= 1000;
  ts.tv_nsec += timeout * 1000000l;
  if (ts.tv_nsec > 1000000000l) {
    ts.tv_nsec -= 1000000000l;
    ++ts.tv_sec;
  }

  // Wait to acquire ressource.
  bool failed(sem_timedwait(&_sem, &ts));
  if (failed && (errno != ETIMEDOUT)) {
    char const* msg(strerror(errno));
    throw(basic_error() << "unable to acquire semaphore: " << msg);
  }
  return (!failed);
#else
  // Implementation based on try_acquire and usleep.

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

  // Wait to acquire ressource.
  bool locked(try_acquire());
  while (!locked && ((now.tv_sec * 1000000ull + now.tv_usec) <
                     (limit.tv_sec * 1000000ull + limit.tv_usec))) {
    usleep(100);
    locked = try_acquire();
    gettimeofday(&now, NULL);
  }

  return (locked);
#endif  // SUSv3, Timeouts option.
}

/**
 *  Get the current number of available ressource.
 *
 *  @return The number of available ressources.
 */
int semaphore::available() {
  int sval(0);
  if (sem_getvalue(&_sem, &sval)) {
    char const* msg(strerror(errno));
    throw(
        basic_error() << "unable to get semaphore's ressource count: " << msg);
  }
  return (sval);
}

/**
 *  Release one ressource.
 */
void semaphore::release() {
  if (sem_post(&_sem)) {
    char const* msg(strerror(errno));
    throw(basic_error() << "unable to release semaphore: " << msg);
  }
  return;
}

/**
 *  Try to acquire one ressource.
 *
 *  @return True if one ressource is acquire, otherwise false.
 */
bool semaphore::try_acquire() {
  bool failed(sem_trywait(&_sem));
  if (failed && (errno != EAGAIN)) {
    char const* msg(strerror(errno));
    throw(basic_error() << "unable to acquire semaphore: " << msg);
  }
  return (!failed);
}
