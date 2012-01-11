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

#include <assert.h>
#include <stdlib.h>
#include <windows.h>
#include "com/centreon/concurrency/mutex_win32.hh"
#include "com/centreon/exceptions/basic.hh"

using namespace com::centreon::concurrency;

/**************************************
*                                     *
*           Public Methods            *
*                                     *
**************************************/

/**
 *  Default constructor.
 */
mutex::mutex() {
  InitializeCriticalSection(&_csection);
}

/**
 *  Destructor.
 */
mutex::~mutex() {
  DeleteCriticalSection(&_csection);
}

/**
 *  Lock the mutex.
 */
void mutex::lock() {
  EnterCriticalSection(&_csection);
  return ;
}

/**
 *  Try to lock the mutex.
 *
 *  @return true if the mutex was successfully acquired.
 */
bool mutex::trylock() {
  return (TryEnterCriticalSection(&_csection) != 0);
}

/**
 *  Unlock the mutex.
 */
void mutex::unlock() {
  LeaveCriticalSection(&_csection);
  return ;
}

/**************************************
*                                     *
*           Private Methods           *
*                                     *
**************************************/

/**
 *  @brief Copy constructor.
 *
 *  Any call to this constructor will result in a call to abort().
 *
 *  @param[in] right Unused.
 */
mutex::mutex(mutex const& right) {
  _internal_copy(right);
}

/**
 *  @brief Assignment operator.
 *
 *  Any call to this method will result in a call to abort().
 *
 *  @param[in] right Unused.
 *
 *  @return This object.
 */
mutex& mutex::operator=(mutex const& right) {
  _internal_copy(right);
  return (*this);
}

/**
 *  Calls abort().
 *
 *  @param[in] right Unused.
 */
void mutex::_internal_copy(mutex const& right) {
  (void)right;
  assert(!"mutex is not copyable");
  abort();
  return ;
}
