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

#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "com/centreon/exception/basic.hh"
#include "com/centreon/concurrency/semaphore.hh"

using namespace com::centreon::concurrency;

/**
 *  Default constructor.
 *
 *  @param[in] n  Specifies the initial value for the semaphore.
 */
semaphore::semaphore(unsigned int n) {
  if (sem_init(&_sem, 0, n))
    throw (basic_error() << "impossible to create semaphore:"
           << strerror(errno));
}

/**
 *  Default destructor.
 */
semaphore::~semaphore() throw () {
  sem_destroy(&_sem);
}

/**
 *  Acquire one ressource. If the semaphore's value is greater than zero
 *  then the function returns immediately, else the call blocks until one
 *  ressource was release.
 */
void semaphore::acquire() {
  // Wait to acquire ressource.
  if (sem_wait(&_sem))
    throw (basic_error() << "impossible to acquire the semaphore:"
           << strerror(errno));
}


/**
 *  This is an overload of acquire.
 *
 *  @param[in] timeout  Define the timeout to wait one ressource.
 *
 *  @return True if one ressource is acquire, false if timeout.
 */
bool semaphore::acquire(unsigned long timeout) {
  timespec ts;
  // Get the current time.
  if (clock_gettime(CLOCK_REALTIME, &ts))
    throw (basic_error() << "failed sleep thread:"
           << strerror(errno));

  // Add the timeout.
  time_t sec(timeout / 1000);
  timeout -= sec * 1000;
  ts.tv_sec += sec;
  ts.tv_nsec += timeout * 1000 * 1000;

  // Wait to acquire ressource.
  if (!sem_timedwait(&_sem, &ts))
    return (true);
  if (errno == ETIMEDOUT)
    return (false);
  throw (basic_error() << "impossible to acquire the semaphore:"
         << strerror(errno));
}

/**
 *  Get the current number of available ressource.
 *
 *  @return The number of available ressources.
 */
int  semaphore::available() {
  int sval(0);
  if (sem_getvalue(&_sem, &sval))
    throw (basic_error() << "impossibel to get the available number:"
           << strerror(errno));
  return (sval);
}

/**
 *  Release one ressource.
 */
void semaphore::release() {
  if (sem_post(&_sem))
    throw (basic_error() << "impossible to release the semaphore:"
           << strerror(errno));
}

/**
 *  Try to acquire one ressource.
 *
 *  @return True if one ressource is acquire, otherwise false.
 */
bool semaphore::try_acquire() {
  if (!sem_trywait(&_sem))
    return (true);
  if (errno == EAGAIN)
    return (false);
  throw (basic_error() << "impossible to try to acquire the " \
         "semaphore:" << strerror(errno));
}

/**
 *  Default copy constructor.
 *
 *  @param[in] right  The object to copy.
 */
semaphore::semaphore(semaphore const& right) {
  _internal_copy(right);
}

/**
 *  Default copy operator.
 *
 *  @param[in] right  The object to copy.
 *
 *  @return This object.
 */
semaphore& semaphore::operator=(semaphore const& right) {
  return (_internal_copy(right));
}

/**
 *  Internal copy.
 *
 *  @param[in] right  The object to copy.
 *
 *  @return This object.
 */
semaphore& semaphore::_internal_copy(semaphore const& right) {
  (void)right;
  assert(!"impossible to copy semaphore");
  abort();
  return (*this);
}
