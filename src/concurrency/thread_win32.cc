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
thread::~thread() throw() { _close(); }

/**
 *  Execute the running method in the new thread.
 */
void thread::exec() {
  _th = CreateThread(NULL, 0, &_helper, this, 0, NULL);
  if (!_th) {
    DWORD errcode(GetLastError());
    throw(basic_error() << "failed to create thread (error " << errcode << ")");
  }
  return;
}

/**
 *  Get the current thread ID.
 *
 *  @return The current thread ID.
 */
thread_id thread::get_current_id() throw() { return (GetCurrentThread()); }

/**
 *  Sleep for some milliseconds.
 *
 *  @param[in] msecs Number of ms to sleep.
 */
void thread::msleep(unsigned long msecs) {
  Sleep(msecs);
  return;
}

/**
 *  Sleep for some nanoseconds.
 *
 *  @param[in] msecs Number of ns to sleep.
 */
void thread::nsleep(unsigned long nsecs) {
  Sleep(nsecs / 1000000);
  return;
}

/**
 *  Sleep for some seconds.
 *
 *  @param[in] secs Number of s to sleep.
 */
void thread::sleep(unsigned long secs) {
  Sleep(secs * 1000);
  return;
}

/**
 *  Sleep for some microseconds.
 *
 *  @param[in] usecs Number of us to sleep.
 */
void thread::usleep(unsigned long usecs) {
  Sleep(usecs / 1000);
  return;
}

/**
 *  Wait for thread termination.
 */
void thread::wait() {
  if (WaitForSingleObject(_th, INFINITE) != WAIT_OBJECT_0) {
    DWORD errcode(GetLastError());
    throw(basic_error() << "failure while waiting thread (error " << errcode
                        << ")");
  }
  return;
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
    throw(basic_error() << "failure while waiting thread (error " << errcode
                        << ")");
  }
  return (success);
}

/**
 *  Release CPU.
 */
void thread::yield() throw() {
  Sleep(0);
  return;
}

/**************************************
*                                     *
*           Private Methods           *
*                                     *
**************************************/

/**
 *  Close thread.
 */
void thread::_close() throw() {
  if (_th) {
    CloseHandle(_th);
    _th = NULL;
  }
  return;
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
