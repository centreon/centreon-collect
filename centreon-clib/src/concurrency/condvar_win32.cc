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
#include "com/centreon/concurrency/condvar_win32.hh"
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
condvar::condvar() {
  InitializeConditionVariable(&_cond);
}

/**
 *  Destructor.
 */
condvar::~condvar() throw () {}

/**
 *  Wait on condition variable.
 *
 *  @param[in] mutx Associated mutex.
 */
void condvar::wait(mutex* mutx) {
  _wait(mutx, INFINITE);
  return ;
}

/**
 *  Overload of wait().
 *
 *  @param[in] mutx    Associated mutex.
 *  @param[in] timeout Maximum number of milliseconds to wait to be
 *                     waked by condition variable.
 */
bool condvar::wait(mutex* mutx, unsigned long timeout) {
  return (_wait(mutx, timeout));
}

/**
 *  Wake all threads waiting on condition variable.
 */
void condvar::wake_all() {
  WakeAllConditionVariable(&_cond);
  return ;
}

/**
 *  Wake at least one thread waiting on the condition variable.
 */
void condvar::wake_one() {
  WakeConditionVariable(&_cond);
  return ;
}

/**************************************
*                                     *
*           Private Methods           *
*                                     *
**************************************/

/**
 *  Wait on condition variable.
 *
 *  @param[in] mutx    Associated mutex.
 *  @param[in] timeout Maximum number of milliseconds to wait.
 *
 *  @return true if mutex was acquired.
 */
bool condvar::_wait(mutex* mutx, DWORD timeout) {
  if (!mutx)
    throw (basic_error() << "wait was called with null mutex");
  bool retval(SleepConditionVariableCS(
                &_cond,
                &mutx->_csection,
                timeout) != 0);
  if (!retval) {
    DWORD errcode(GetLastError());
    if (errcode != WAIT_TIMEOUT)
      throw (basic_error() << "failed to wait on condition variable " \
               "(error " << errcode << ")");
  }
  return (retval);
}
