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
  assert(!"readers-writer lock is not copyable");
  abort();
  return ;
}
