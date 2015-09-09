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

#include <cstdlib>
#include "com/centreon/concurrency/locker.hh"
#include "com/centreon/concurrency/read_write_lock_win32.hh"

using namespace com::centreon::concurrency;

/**************************************
*                                     *
*           Public Methods            *
*                                     *
**************************************/

/**
 *  Default constructor.
 */
read_write_lock::read_write_lock()
  : _readers(0),
    _readers_waiting(0),
    _writer(false),
    _writers_waiting(0) {}

/**
 *  Destructor.
 */
read_write_lock::~read_write_lock() throw () {}

/**
 *  Lock RWL for reading.
 */
void read_write_lock::read_lock() {
  locker lock(&_mtx);
  if (_writer || _writers_waiting) {
    ++_readers_waiting;
    do {
      _condvar.wait(&_mtx);
    } while (_writer || _writers_waiting);
    --_readers_waiting;
  }
  ++_readers;
  return ;
}

/**
 *  Lock RWL for reading without waiting more than some time.
 *
 *  @param[in] timeout Maximum time in milliseconds to wait.
 *
 *  @return true if RWL was successfully locked.
 */
bool read_write_lock::read_lock(unsigned long timeout) {
  locker lock(&_mtx);
  if (_writer || _writers_waiting) {
    ++_readers_waiting;
    DWORD now(timeGetTime());
    DWORD limit(now + timeout);
    do {
      _condvar.wait(&_mtx, limit - now);
      if (!_writer && !_writers_waiting)
        break ;
      now = timeGetTime();
    } while (now < limit);
    --_readers_waiting;
  }
  bool retval;
  if (_writer || _writers_waiting)
    retval = false;
  else {
    ++_readers;
    retval = true;
  }
  return (retval);
}

/**
 *  Try to lock RWL for reading.
 *
 *  @return true if RWL was locked.
 */
bool read_write_lock::read_trylock() {
  locker lock(&_mtx);
  bool retval;
  if (_writer || _writers_waiting)
    retval = false;
  else {
    ++_readers;
    retval = true;
  }
  return (retval);
}

/**
 *  Release read lock.
 */
void read_write_lock::read_unlock() {
  locker lock(&_mtx);
  --_readers;
  if (_writers_waiting)
    _condvar.wake_one();
  return ;
}

/**
 *  Lock RWL for writing.
 */
void read_write_lock::write_lock() {
  locker lock(&_mtx);
  if (_readers || _writer) {
    ++_writers_waiting;
    do {
      _condvar.wait(&_mtx);
    } while (_readers || _writer);
    --_writers_waiting;
  }
  _writer = true;
  return ;
}

/**
 *  Lock RWL for writing without waiting more than some time.
 *
 *  @param[in] timeout Maximum time in milliseconds to wait.
 *
 *  @return true if RWL was successfully locked.
 */
bool read_write_lock::write_lock(unsigned long timeout) {
  locker lock(&_mtx);
  if (_readers || _writer) {
    ++_writers_waiting;
    DWORD now(timeGetTime());
    DWORD limit(now + timeout);
    do {
      _condvar.wait(&_mtx, limit - now);
      if (!_readers || !_writer)
        break ;
      now = timeGetTime();
    } while (now < limit);
  }
  bool retval;
  if (_readers || _writer)
    retval = false;
  else {
    _writer = true;
    retval = true;
  }
  return (retval);
}

/**
 *  Try to lock RWL for writing.
 *
 *  @return true if RWL was locked.
 */
bool read_write_lock::write_trylock() {
  locker lock(&_mtx);
  bool retval;
  if (_readers || _writer)
    retval = false;
  else {
    _writer = true;
    retval = true;
  }
  return (retval);
}

/**
 *  Release write lock.
 */
void read_write_lock::write_unlock() {
  locker lock(&_mtx);
  _writer = false;
  if (_writers_waiting)
    _condvar.wake_one();
  else if (_readers_waiting)
    _condvar.wake_all();
  return ;
}
