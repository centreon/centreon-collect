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
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "com/centreon/exceptions/basic.hh"
#include "com/centreon/concurrency/mutex.hh"

using namespace com::centreon::concurrency;

/**
 *  Default constructor.
 */
mutex::mutex() {
  int ret(pthread_mutex_init(&_mtx, NULL));
  if (ret)
    throw (basic_error() << "impossible to create mutex:"
           << strerror(ret));
}

/**
 *  Default destructor.
 */
mutex::~mutex() throw () {
  pthread_mutex_destroy(&_mtx);
}

/**
 *  Lock the mutex and if another thread has already locked the mutex
 *  then this call will block until the mutex has unlock by the first
 *  thread.
 */
void mutex::lock() {
  int ret(pthread_mutex_lock(&_mtx));
  if (ret)
    throw (basic_error() << "the mutex lock failed:" << strerror(ret));
}

/**
 *  Lock the mutex if the mutex is unlock and return without any
 *  modification on the mutex if anobther thread has already locked the
 *  mutex.
 *
 *  @return True if the mutex is lock, false if the mutex was already
 *  lock.
 */
bool mutex::trylock() {
  int ret(pthread_mutex_trylock(&_mtx));
  if (!ret)
    return (true);
  if (ret == EBUSY)
    return (false);
  throw (basic_error() << "the mutex trylock failed:"
         << strerror(ret));
}

/**
 *  Unlock the mutex.
 */
void mutex::unlock() {
  int ret(pthread_mutex_unlock(&_mtx));
  if (ret)
    throw (basic_error() << "the mutex unlock failed:"
           << strerror(ret));
}

/**
 *  Default copy constructor.
 *
 *  @param[in] right  The object to copy.
 */
mutex::mutex(mutex const& right) {
  _internal_copy(right);
}

/**
 *  Default copy operator.
 *
 *  @param[in] right  The object to copy.
 *
 *  @return This object.
 */
mutex& mutex::operator=(mutex const& right) {
  return (_internal_copy(right));
}

/**
 *  Internal copy.
 *
 *  @param[in] right  The object to copy.
 *
 *  @return This object.
 */
mutex& mutex::_internal_copy(mutex const& right) {
  (void)right;
  assert(!"impossible to copy mutex.");
  abort();
  return (*this);
}
