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
#include <windows.h>
#include "com/centreon/concurrency/thread_win32.hh"
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
thread::thread() : _th(NULL) {}

/**
 *  Destructor.
 */
thread::~thread() throw () {
  _close();
}

/**
 *  Execute the running method in the new thread.
 */
void thread::exec() {
  _th = CreateThread(NULL, 0, &_helper, this, 0, NULL);
  if (!_th) {
    DWORD errcode(GetLastError());
    throw (basic_error() << "failed to create thread (error "
           << errcode << ")");
  }
  return ;
}

/**
 *  Get the current thread ID.
 *
 *  @return The current thread ID.
 */
thread_id thread::get_current_id() throw () {
  return (GetCurrentThread());
}

/**
 *  Sleep for some milliseconds.
 *
 *  @param[in] msecs Number of ms to sleep.
 */
void thread::msleep(unsigned long msecs) {
  Sleep(msecs);
  return ;
}

/**
 *  Sleep for some nanoseconds.
 *
 *  @param[in] msecs Number of ns to sleep.
 */
void thread::nsleep(unsigned long nsecs) {
  Sleep(nsecs / 1000000);
  return ;
}

/**
 *  Sleep for some seconds.
 *
 *  @param[in] secs Number of s to sleep.
 */
void thread::sleep(unsigned long secs) {
  Sleep(secs * 1000);
  return ;
}

/**
 *  Sleep for some microseconds.
 *
 *  @param[in] usecs Number of us to sleep.
 */
void thread::usleep(unsigned long usecs) {
  Sleep(usecs * 1000);
  return ;
}

/**
 *  Wait for thread termination.
 */
void thread::wait() {
  if (WaitForSingleObject(_th, INFINITE) != WAIT_OBJECT_0) {
    DWORD errcode(GetLastError());
    throw (basic_error() << "failure while waiting thread (error "
           << errcode << ")");
  }
  return ;
}

/**
 *  Wait for thread termination or until timeout elapse.
 *
 *  @param[in] timeout Time in milliseconds to wait the thread at most.
 *
 *  @return true if the thread ends before timeout, otherwise false.
 */
bool thread::wait(unsigned long timeout) {
  DWORD ret(WaitForSingleObject(_th, timeout));
  bool success(ret == WAIT_OBJECT_0);
  if (!success && (ret != WAIT_TIMEOUT)) {
    DWORD errcode(GetLastError());
    throw (basic_error() << "failure while waiting thread (error "
           << errcode << ")");
  }
  return (success);
}

/**
 *  Release CPU.
 */
void thread::yield() throw () {
  Sleep(0);
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
 *  @param[in] t Unused.
 */
thread::thread(thread const& t) {
  _internal_copy(t);
}

/**
 *  @brief Assignment operator.
 *
 *  Any call to this method will result in call to abort().
 *
 *  @param[in] t Unused.
 *
 *  @return This object.
 */
thread& thread::operator=(thread const& t) {
  _internal_copy(t);
  return (*this);
}

/**
 *  Close thread.
 */
void thread::_close() throw () {
  if (_th) {
    CloseHandle(_th);
    _th = NULL;
  }
  return ;
}

/**
 *  @brief Execution helper.
 *
 *  This static method will be the native thread entry point that will
 *  in turn call the run() method.
 *
 *  @param[in] data Class pointer.
 *
 *  @return 0.
 */
DWORD thread::_helper(void* data) {
  thread* self(static_cast<thread*>(data));
  self->_run();
  return (0);
}

/**
 *  Calls abort().
 *
 *  @param[in] t Unused.
 */
void thread::_internal_copy(thread const& t) {
  (void)t;
  assert(!"thread is not copyable");
  abort();
  return ;
}
