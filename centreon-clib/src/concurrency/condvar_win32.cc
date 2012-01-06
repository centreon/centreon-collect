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
 *  Copy constructor.
 *
 *  @param[in] cv Unused.
 */
condvar::condvar(condvar const& cv) {
  _internal_copy(cv);
}

/**
 *  Assignment operator.
 *
 *  @param[in] cv Unused.
 *
 *  @return This object.
 */
condvar& condvar::operator=(condvar const& cv) {
  _internal_copy(cv);
  return (*this);
}

/**
 *  Copy internal data members.
 *
 *  @param[in] cv Object to copy.
 */
void condvar::_internal_copy(condvar const& cv) {
  (void)cv;
  assert(!"condition variable is not copyable");
  abort();
  return ;
}

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
                timeout));
  if (!retval) {
    DWORD errcode(GetLastError());
    if (errcode != WAIT_TIMEOUT)
      throw (basic_error() << "failed to wait on condition variable " \
               "(error " << errcode << ")");
  }
  return (retval);
}
