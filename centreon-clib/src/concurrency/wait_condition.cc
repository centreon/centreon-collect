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
#include <string.h>
#include <errno.h>
#include <time.h>
#include "com/centreon/exception/basic.hh"
#include "com/centreon/concurrency/wait_condition.hh"

using namespace com::centreon::concurrency;

/**
 *  Default constructor.
 */
wait_condition::wait_condition() {
  int ret(pthread_cond_init(&_cnd, NULL));
  if (ret)
    throw (basic_error() << "the wait condition initialization failed:"
           << strerror(ret));
}

/**
 *  Default destructor.
 */
wait_condition::~wait_condition() throw () {
  pthread_cond_destroy(&_cnd);
}

/**
 *  Wait the condition variable.
 *
 *  @param[in] mtx  The mutex to wait.
 */
void wait_condition::wait(mutex* mtx) {
  if (!mtx)
    throw (basic_error() << "wait was call with invalid argument:" \
           "null pointer");

  int ret(pthread_cond_wait(&_cnd, &mtx->_mtx));
  if (ret)
    throw (basic_error() << "the wait condition wait failed:"
           << strerror(ret));
}

/**
 *  This method is an overload of wait.
 *
 *  @param[in] mtx      The mutex to wait.
 *  @param[in] timeout  The time limit to wait.
 *
 *  @return True if the mutex was take, false if timeout.
 */
bool wait_condition::wait(mutex* mtx, unsigned long timeout) {
  if (!mtx)
    throw (basic_error() << "wait was call with invalid argument:" \
           "null pointer");

  // Get the current time.
  timespec ts;
  if (clock_gettime(CLOCK_REALTIME, &ts))
    throw (basic_error() << "failed sleep thread:"
           << strerror(errno));

  // Add timout.
  time_t sec(timeout / 1000);
  timeout -= sec * 1000;
  ts.tv_sec += sec;
  ts.tv_nsec += timeout * 1000 * 1000;

  // Wait the condition variable.
  int ret(pthread_cond_timedwait(&_cnd, &mtx->_mtx, &ts));
  if (!ret)
    return (true);
  if (ret == ETIMEDOUT)
    return (false);
  throw (basic_error() << "the wait condition wait with timeout failed:"
         << strerror(ret));
}

/**
 *  Wake all condition variable.
 */
void wait_condition::wake_all() {
  int ret(pthread_cond_broadcast(&_cnd));
  if (ret)
    throw (basic_error() << "the wait condition wake all failed:"
           << strerror(ret));
}

/**
 *  Wake one condition variable.
 */
void wait_condition::wake_one() {
  int ret(pthread_cond_signal(&_cnd));
  if (ret)
    throw (basic_error() << "the wait condition wake one failed:"
           << strerror(ret));
}

/**
 *  Default copy constructor.
 *
 *  @param[in] right  The object to copy.
 */
wait_condition::wait_condition(wait_condition const& right) {
  _internal_copy(right);
}

/**
 *  Default copy operator.
 *
 *  @param[in] right  The object to copy.
 *
 *  @return This object.
 */
wait_condition& wait_condition::operator=(wait_condition const& right) {
  return (_internal_copy(right));
}

/**
 *  Internal copy.
 *
 *  @param[in] right  The object to copy.
 *
 *  @return This object.
 */
wait_condition& wait_condition::_internal_copy(wait_condition const& right) {
  (void)right;
  assert(!"impossible to copy wait_condition");
  abort();
  return (*this);
}
