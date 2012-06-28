/*
** Copyright 2012 Merethis
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
#include <pthread.h>
#include "com/centreon/exceptions/basic.hh"
#include "com/centreon/concurrency/read_write_lock_posix.hh"

using namespace com::centreon::concurrency;

/**************************************
*                                     *
*           Public Methods            *
*                                     *
**************************************/

/**
 *  Default constructor.
 */
read_write_lock::read_write_lock() {
  // Return value.
  int ret;

  // Initialize RWL.
  ret = pthread_rwlock_init(&_rwl, NULL);
  if (ret)
    throw (basic_error() << "cannot initialize readers-writer lock: "
           << strerror(ret));
}

/**
 *  Destructor.
 */
read_write_lock::~read_write_lock() throw () {
  pthread_rwlock_destroy(&_rwl);
}

/**
 *  Read (shared) lock.
 */
void read_write_lock::read_lock() {
  int ret(pthread_rwlock_rdlock(&_rwl));
  if (ret)
    throw (basic_error() << "cannot lock readers-writer lock: "
           << strerror(ret));
  return ;
}

/**
 *  Try to get read (shared) lock with a timeout.
 *
 *  @param[in] timeout Maximum number of milliseconds to wait for
 *                     availability.
 *
 *  @return true if RWL was successfully locked.
 */
bool read_write_lock::read_lock(unsigned long timeout) {
  // Get the current time.
  timespec ts;
  if (clock_gettime(CLOCK_REALTIME, &ts)) {
    char const* msg(strerror(errno));
    throw (basic_error()
           << "unable to get time within readers-writer lock: " << msg);
  }

  // Add the timeout.
  ts.tv_sec += timeout / 1000;
  timeout %= 1000;
  ts.tv_nsec += timeout * 1000000l;
  if (ts.tv_nsec >= 1000000000l) {
    ts.tv_nsec -= 1000000000l;
    ++ts.tv_sec;
  }

  // Wait to acquire lock.
  int ret(pthread_rwlock_timedrdlock(&_rwl, &ts));
  if (ret && (ret != ETIMEDOUT))
    throw (basic_error() << "cannot lock readers-writer lock: "
           << strerror(ret));

  return (ret != ETIMEDOUT);
}

/**
 *  @brief Try to get read (shared) lock.
 *
 *  Function will return instantly.
 *
 *  @return true if lock was acquired.
 */
bool read_write_lock::read_trylock() {
  int ret(pthread_rwlock_tryrdlock(&_rwl));
  if (ret && (ret != EBUSY))
    throw (basic_error() << "cannot lock readers-writer lock: "
           << strerror(ret));
  return (ret != EBUSY);
}

/**
 *  Unlock previously acquired lock.
 */
void read_write_lock::read_unlock() {
  int ret(pthread_rwlock_unlock(&_rwl));
  if (ret)
    throw (basic_error() << "cannot unlock readers-writer lock: "
           << strerror(ret));
  return ;
}

/**
 *  Write (exclusive) lock.
 */
void read_write_lock::write_lock() {
  int ret(pthread_rwlock_wrlock(&_rwl));
  if (ret)
    throw (basic_error() << "cannot lock readers-writer lock: "
           << strerror(ret));
  return ;
}

/**
 *  Try to get write (exclusive) lock with a timeout.
 *
 *  @param[in] timeout Maximum number of milliseconds to wait for
 *                     availability.
 *
 *  @return true if RWL was successfully locked.
 */
bool read_write_lock::write_lock(unsigned long timeout) {
  // Get the current time.
  timespec ts;
  if (clock_gettime(CLOCK_REALTIME, &ts)) {
    char const* msg(strerror(errno));
    throw (basic_error()
           << "unable to get time within readers-writer lock: " << msg);
  }

  // Add the timeout.
  ts.tv_sec += timeout / 1000;
  timeout %= 1000;
  ts.tv_nsec += timeout * 1000000l;
  if (ts.tv_nsec >= 1000000000l) {
    ts.tv_nsec -= 1000000000l;
    ++ts.tv_sec;
  }

  // Wait to acquire lock.
  int ret(pthread_rwlock_timedwrlock(&_rwl, &ts));
  if (ret && (ret != ETIMEDOUT))
    throw (basic_error() << "cannot lock readers-writer lock: "
           << strerror(ret));

  return (ret != ETIMEDOUT);
}

/**
 *  @brief Try to get write (exclusive) lock.
 *
 *  Function will return instantly.
 *
 *  @return true if lock was acquired.
 */
bool read_write_lock::write_trylock() {
  int ret(pthread_rwlock_trywrlock(&_rwl));
  if (ret && (ret != EBUSY))
    throw (basic_error() << "cannot lock readers-writer lock: "
           << strerror(ret));
  return (ret != EBUSY);
}

/**
 *  Unlock previously acquired lock.
 */
void read_write_lock::write_unlock() {
  int ret(pthread_rwlock_unlock(&_rwl));
  if (ret)
    throw (basic_error() << "cannot unlock readers-writer lock: "
           << strerror(ret));
  return ;
}

/**************************************
*                                     *
*           Private Methods           *
*                                     *
**************************************/

/**
 *  Copy constructor.
 *
 *  @param[in] right Object to copy.
 */
read_write_lock::read_write_lock(read_write_lock const& right) {
  _internal_copy(right);
}

/**
 *  Assignment operator.
 *
 *  @param[in] right Object to copy.
 *
 *  @return This object.
 */
read_write_lock& read_write_lock::operator=(
                                    read_write_lock const& right) {
  _internal_copy(right);
  return (*this);
}

/**
 *  Copy internal data members.
 *
 *  @param[in] right Object to copy.
 */
void read_write_lock::_internal_copy(read_write_lock const& right) {
  (void)right;
  assert(!"readers/writer lock is not copyable");
  abort();
  return ;
}
