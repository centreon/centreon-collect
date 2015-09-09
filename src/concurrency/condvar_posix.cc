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
#include "com/centreon/exceptions/basic.hh"
#include "com/centreon/concurrency/condvar_posix.hh"

using namespace com::centreon::concurrency;

/**************************************
*                                     *
*           Public Methods            *
*                                     *
**************************************/

/**
 *  Default constructor.
 */
condvar::condvar() {
  int ret(pthread_cond_init(&_cnd, NULL));
  if (ret)
    throw (basic_error() << "could not initialize condition variable: "
           << strerror(ret));
}

/**
 *  Default destructor.
 */
condvar::~condvar() throw () {
  pthread_cond_destroy(&_cnd);
}

/**
 *  Wait the condition variable.
 *
 *  @param[in] mtx  The mutex to wait.
 */
void condvar::wait(mutex* mtx) {
  if (!mtx)
    throw (basic_error() << "wait was called with null mutex");

  int ret(pthread_cond_wait(&_cnd, &mtx->_mtx));
  if (ret)
    throw (basic_error() << "failed to wait on condition variable: "
           << strerror(ret));
  return ;
}

/**
 *  This method is an overload of wait.
 *
 *  @param[in] mtx      The mutex to wait.
 *  @param[in] timeout  The time limit to wait.
 *
 *  @return True if the mutex was take, false if timeout.
 */
bool condvar::wait(mutex* mtx, unsigned long timeout) {
  if (!mtx)
    throw (basic_error() << "wait was called with null mutex");

  // Get the current time.
  timespec ts;
  if (clock_gettime(CLOCK_REALTIME, &ts)) {
    char const* msg(strerror(errno));
    throw (basic_error() << "failed to wait on condition variable: "
           << msg);
  }

  // Add timeout.
  ts.tv_sec += timeout / 1000;
  timeout %= 1000;
  ts.tv_nsec += timeout * 1000000l;
  if (ts.tv_nsec > 1000000000l) {
    ts.tv_nsec -= 1000000000l;
    ++ts.tv_sec;
  }

  // Wait the condition variable.
  int ret(pthread_cond_timedwait(&_cnd, &mtx->_mtx, &ts));
  if (ret && (ret != ETIMEDOUT))
    throw (basic_error() << "failed to wait on condition variable: "
           << strerror(ret));
  return (!ret);
}

/**
 *  Wake all condition variable.
 */
void condvar::wake_all() {
  int ret(pthread_cond_broadcast(&_cnd));
  if (ret)
    throw (basic_error() << "could not wake all threads attached to " \
                "condition variable: " << strerror(ret));
  return ;
}

/**
 *  Wake one condition variable.
 */
void condvar::wake_one() {
  int ret(pthread_cond_signal(&_cnd));
  if (ret)
    throw (basic_error() << "could not wake one thread attached to " \
                "condition variable: " << strerror(ret));
}
