/*
** Copyright 2012-2013 Centreon
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
    throw(basic_error() << "cannot initialize readers-writer lock: "
                        << strerror(ret));
}

/**
 *  Destructor.
 */
read_write_lock::~read_write_lock() throw() { pthread_rwlock_destroy(&_rwl); }

/**
 *  Read (shared) lock.
 */
void read_write_lock::read_lock() {
  int ret(pthread_rwlock_rdlock(&_rwl));
  if (ret)
    throw(
        basic_error() << "cannot lock readers-writer lock: " << strerror(ret));
  return;
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
    throw(basic_error() << "unable to get time within readers-writer lock: "
                        << msg);
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
    throw(
        basic_error() << "cannot lock readers-writer lock: " << strerror(ret));

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
    throw(
        basic_error() << "cannot lock readers-writer lock: " << strerror(ret));
  return (ret != EBUSY);
}

/**
 *  Unlock previously acquired lock.
 */
void read_write_lock::read_unlock() {
  int ret(pthread_rwlock_unlock(&_rwl));
  if (ret)
    throw(basic_error() << "cannot unlock readers-writer lock: "
                        << strerror(ret));
  return;
}

/**
 *  Write (exclusive) lock.
 */
void read_write_lock::write_lock() {
  int ret(pthread_rwlock_wrlock(&_rwl));
  if (ret)
    throw(
        basic_error() << "cannot lock readers-writer lock: " << strerror(ret));
  return;
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
    throw(basic_error() << "unable to get time within readers-writer lock: "
                        << msg);
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
    throw(
        basic_error() << "cannot lock readers-writer lock: " << strerror(ret));

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
    throw(
        basic_error() << "cannot lock readers-writer lock: " << strerror(ret));
  return (ret != EBUSY);
}

/**
 *  Unlock previously acquired lock.
 */
void read_write_lock::write_unlock() {
  int ret(pthread_rwlock_unlock(&_rwl));
  if (ret)
    throw(basic_error() << "cannot unlock readers-writer lock: "
                        << strerror(ret));
  return;
}
