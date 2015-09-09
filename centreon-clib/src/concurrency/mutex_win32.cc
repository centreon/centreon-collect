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
