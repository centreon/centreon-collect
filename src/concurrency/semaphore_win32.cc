/*
** Copyright 2011-2013 Centreon
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
    throw(basic_error() << "unable to create semaphore (error " << errcode
                        << ")");
  }
}

/**
 *  Destructor.
 */
semaphore::~semaphore() throw() { CloseHandle(_sem); }

/**
 *  Acquire one ressource. If the semaphore's value is greater then zero
 *  then the function returns immediately, else the call blocks until
 *  one ressource is released.
 */
void semaphore::acquire() {
  DWORD ret(WaitForSingleObject(_sem, INFINITE));
  if (ret != WAIT_OBJECT_0)
    throw(basic_error() << "unable to acquire semaphore (error " << ret << ")");
  return;
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
    throw(basic_error() << "unable to acquire semaphore (error " << ret << ")");
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
    throw(basic_error() << "unable to get semaphore count (error " << errcode
                        << ")");
  }
  return (count);
}

/**
 *  Release one ressource.
 */
void semaphore::release() {
  if (!ReleaseSemaphore(_sem, 1, NULL)) {
    int errcode(GetLastError());
    throw(basic_error() << "unable to release semaphore (error " << errcode
                        << ")");
  }
  return;
}

/**
 *  Try to acquire one ressource.
 *
 *  @return true if one ressource was acquired.
 */
bool semaphore::try_acquire() { return (acquire(0)); }
