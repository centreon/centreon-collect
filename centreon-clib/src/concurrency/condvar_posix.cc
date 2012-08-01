/*
** Copyright 2011-2012 Merethis
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

#include <cassert>
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

/**************************************
*                                     *
*           Private Methods           *
*                                     *
**************************************/

/**
 *  Copy constructor.
 *
 *  @param[in] right  The object to copy.
 */
condvar::condvar(condvar const& right) {
  _internal_copy(right);
}

/**
 *  Assignment operator.
 *
 *  @param[in] right  The object to copy.
 *
 *  @return This object.
 */
condvar& condvar::operator=(condvar const& right) {
  _internal_copy(right);
  return (*this);
}

/**
 *  Internal copy.
 *
 *  @param[in] right  The object to copy.
 */
void condvar::_internal_copy(condvar const& right) {
  (void)right;
  assert(!"cannot copy condition variable");
  abort();
  return ;
}
