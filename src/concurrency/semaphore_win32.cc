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
#include <climits>
#include <cstdlib>
#include <windows.h>
#include "com/centreon/exceptions/basic.hh"
#include "com/centreon/concurrency/semaphore_win32.hh"

using namespace com::centreon::concurrency;

/**************************************
*                                     *
*           Public Methods            *
*                                     *
**************************************/

/**
 *  Default constructor.
 *
 *  @param[in] n Specified the initial value for the semaphore.
 */
semaphore::semaphore(unsigned int n) {
  _sem = CreateSemaphore(NULL, n, LONG_MAX, NULL);
  if (!_sem) {
    int errcode(GetLastError());
    throw (basic_error() << "unable to create semaphore (error "
           << errcode << ")");
  }
}

/**
 *  Destructor.
 */
semaphore::~semaphore() throw () {
  CloseHandle(_sem);
}

/**
 *  Acquire one ressource. If the semaphore's value is greater then zero
 *  then the function returns immediately, else the call blocks until
 *  one ressource is released.
 */
void semaphore::acquire() {
  DWORD ret(WaitForSingleObject(_sem, INFINITE));
  if (ret != WAIT_OBJECT_0)
    throw (basic_error() << "unable to acquire semaphore (error "
           << ret << ")");
  return ;
}

/**
 *  This is an overload of acquire().
 *
 *  @param[in] timeout Maximum number of milliseconds to wait for
 *                     ressource availability.
 *
 *  @return true if ressource was acquired.
 */
bool semaphore::acquire(unsigned long timeout) {
  DWORD ret(WaitForSingleObject(_sem, timeout));
  bool success(ret == WAIT_OBJECT_0);
  if (!success && (ret != WAIT_TIMEOUT))
    throw (basic_error() << "unable to acquire semaphore (error "
           << ret << ")");
  return (success);
}

/**
 *  Get the current number of available ressources.
 *
 *  @return Number of ressources available.
 */
int semaphore::available() {
  LONG count;
  if (!ReleaseSemaphore(_sem, 0, &count)) {
    int errcode(GetLastError());
    throw (basic_error() << "unable to get semaphore count (error "
           << errcode << ")");
  }
  return (count);
}

/**
 *  Release one ressource.
 */
void semaphore::release() {
  if (!ReleaseSemaphore(_sem, 1, NULL)) {
    int errcode(GetLastError());
    throw (basic_error() << "unable to release semaphore (error "
           << errcode << ")");
  }
  return ;
}

/**
 *  Try to acquire one ressource.
 *
 *  @return true if one ressource was acquired.
 */
bool semaphore::try_acquire() {
  return (acquire(0));
}

/**************************************
*                                     *
*           Private Methods           *
*                                     *
**************************************/

/**
 *  Copy constructor.
 *
 *  @param[in] s Object to copy.
 */
semaphore::semaphore(semaphore const& s) {
  _internal_copy(s);
}

/**
 *  Assignment operator.
 *
 *  @param[in] s Object to copy.
 *
 *  @return This object.
 */
semaphore& semaphore::operator=(semaphore const& s) {
  _internal_copy(s);
  return (*this);
}

/**
 *  Copy internal data members.
 *
 *  @param[in] s Object to copy.
 */
void semaphore::_internal_copy(semaphore const& s) {
  (void)s;
  assert(!"semaphore is not copyable");
  abort();
  return ;
}
