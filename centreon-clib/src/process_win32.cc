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
#include <cstring>
#include <windows.h>
#include "com/centreon/exceptions/basic.hh"
#include "com/centreon/process_win32.hh"
#include "com/centreon/unique_array_ptr.hh"

using namespace com::centreon;

/**************************************
*                                     *
*           Public Methods            *
*                                     *
**************************************/

/**
 *  Default constructor.
 */
process::process(process_listener* listener)
    : _exit_code(0),
      _is_timeout(false),
      _listener(listener),
      _process(NULL),
      _timeout(0) {
  memset(_enable_stream, 1, sizeof(_enable_stream));
  memset(_stream, NULL, sizeof(_stream));
}

/**
 *  Destructor.
 */
process::~process() throw() {
  kill();
  wait();
}

/**
 *  Enable or disable process' stream.
 *
 *  @param[in] s      The stream to set.
 *  @param[in] enable Set to true to enable stderr.
 */
void process::enable_stream(stream s, bool enable) {
  concurrency::locker lock(&_lock_process);
  if (_enable_stream[s] != enable) {
    if (!_process)
      _enable_stream[s] = enable;
    else if (!enable)
      _close(_stream[s]);
    else
      throw(basic_error() << "cannot reenable \"" << s
                          << "\" while process is running");
  }
  return;
}

/**
 *  Get the time when the process execution finished.
 *
 *  @return The ending timestamp.
 */
timestamp const& process::end_time() const throw() {
  concurrency::locker lock(&_lock_process);
  return (_end_time);
}

/**
 *  Run process.
 *
 *  @param[in] cmd     Command line.
 *  @param[in] env     Array of strings (on form key=value), which are
 *                     passed as environment to the new process. If env
 *                     is NULL, the current environement are passed of
 *                     the new process.
 *  @param[in] timeout Maximum time in seconde to execute process. After
 *                     this time the process will be kill.
 */
void process::exec(char const* cmd, char** env, unsigned int timeout) {
  concurrency::locker lock(&_lock_process);

  // Check viability.
  if (_process)
    throw(basic_error() << "process " << _process->dwProcessId
                        << " is already started and has not been waited");

  // Reset exit code.
  _buffer_err.clear();
  _buffer_out.clear();
  _end_time.clear();
  _exit_code = 0;
  _is_timeout = false;
  _start_time.clear();
  _timeout = timeout;

  HANDLE child_stream[3] = {NULL, NULL, NULL};

  try {
    // Create pipes if necessary.
    if (_enable_stream[in]) {
      _pipe(&child_stream[in], &_stream[in]);
      SetHandleInformation(_stream[in], HANDLE_FLAG_INHERIT, 0);
    }
    if (_enable_stream[out]) {
      _pipe(&_stream[out], &child_stream[out]);
      SetHandleInformation(_stream[out], HANDLE_FLAG_INHERIT, 0);
    }
    if (_enable_stream[err]) {
      _pipe(&_stream[err], &child_stream[err]);
      SetHandleInformation(_stream[err], HANDLE_FLAG_INHERIT, 0);
    }

    if (!env)
      env = environ;

    // Set startup informations.
    STARTUPINFO si;
    memset(&si, 0, sizeof(si));
    si.cb = sizeof(si);
    si.hStdError = child_stream[err];
    si.hStdInput = child_stream[in];
    si.hStdOutput = child_stream[out];
    si.dwFlags = STARTF_USESTDHANDLES;

    // Execute process.
    _process = new PROCESS_INFORMATION;
    memset(_process, 0, sizeof(*_process));
    unsigned int size(strlen(cmd) + 1);
    unique_array_ptr<char> cmd_str(new char[size]);
    memcpy(cmd_str.get(), cmd, size);
    if (CreateProcess(NULL,
                      cmdstr.get(),
                      NULL,
                      NULL,
                      TRUE,
                      0,
                      env,
                      NULL,
                      &si,
                      _process) == FALSE) {
      int errcode(GetLastError());
      throw(basic_error() << "could not create process (error " << errcode
                          << ")");
    }

    _start_time = timestamp::now();

    for (unsigned int i(0); i < 3; ++i)
      _close(child_stream[i]);

    process_manager::instance().add(this);
  }
  catch (...) {
    delete _process;
    _process = NULL;
    for (unsigned int i(0); i < 3; ++i) {
      _close(child_stream[i]);
      _close(_stream[i]);
    }
    throw;
  }
  return;
}

/**
 *  Run process.
 *
 *  @param[in] cmd     Command line.
 *  @param[in] timeout Maximum time in seconde to execute process. After
 *                     this time the process will be kill.
 */
void process::exec(std::string const& cmd, unsigned int timeout) {
  exec(cmd.c_str(), NULL, timeout);
  return;
}

/**
 *  Get the exit code, return by the executed process.
 *
 *  @return The exit code.
 */
int process::exit_code() const throw() {
  concurrency::locker lock(&_lock_process);
  return (_exit_code);
}

/**
 *  Get the exit status, always return normal into windows.
 *
 *  @return The exit status.
 */
process::status process::exit_status() const throw() {
  concurrency::locker lock(&_lock_process);
  if (_is_timeout)
    return (timeout);
  return (normal);
}

/**
 *  Kill process.
 */
void process::kill() {
  concurrency::locker lock(&_lock_process);
  if (_process) {
    if (!TerminateProcess(_process->hProcess, EXIT_FAILURE)) {
      int errcode(GetLastError());
      throw(basic_error() << "could not terminate process "
                          << _process->dwProcessId << " (error " << errcode
                          << ")");
    }
  }
}

/**
 *  Read data from stdout.
 *
 *  @param[in] data Destination buffer.
 */
void process::read(std::string& data) {
  concurrency::locker lock(&_lock_process);
  if (_buffer_out.empty() && _stream[out] != -1)
    _cv_buffer_out.wait(&_lock_process);
  data.clear();
  data.swap(_buffer_out);
  return;
}

/**
 *  Read data from stderr.
 *
 *  @param[in] data Destination buffer.
 */
void process::read_err(std::string& data) {
  concurrency::locker lock(&_lock_process);
  if (_buffer_err.empty() && _stream[err] != -1)
    _cv_buffer_err.wait(&_lock_process);
  data.clear();
  data.swap(_buffer_err);
  return;
}

/**
 *  Get the time when the process execution start.
 *
 *  @return The starting timestamp.
 */
timestamp const& process::start_time() const throw() {
  concurrency::locker lock(&_lock_process);
  return (_start_time);
}

/**
 *  Terminate the process.
 */
void process::terminate() {
  concurrency::locker lock(&_lock_process);
  if (_process) {
    EnumWindows(&_terminate_window, static_cast<LPARAM>(_process->dwProcessId));
    PostThreadMessage(_process->dwThreadId, WM_CLOSE, 0, 0);
  }
  return;
}

/**
 *  Wait for process termination.
 */
void process::wait() {
  concurrency::locker lock(&_lock_process);
  if (_is_running())
    _cv_process.wait(&_lock_process);
  return;
}

/**
 *  Wait for process termination.
 *
 *  @param[in]  timeout   Maximum number of milliseconds to wait for
 *                        process termination.
 *  @return true if process exited.
 */
bool process::wait(unsigned long timeout) {
  concurrency::locker lock(&_lock_process);
  if (!_is_running())
    return (true);
  _cv_process.wait(&_lock_process, timeout);
  return (!_is_running());
}

/**
 *  Write data to process' standard input.
 *
 *  @param[in] data Source buffer.
 *
 *  @return Number of bytes actually written.
 */
unsigned int process::write(std::string const& data) {
  return (write(data.c_str(), data.size()));
}

/**
 *  Write data to process' standard input.
 *
 *  @param[in] data Source buffer.
 *  @param[in] size Maximum number of bytes to write.
 *
 *  @retun Number of bytes actually written.
 */
unsigned int process::write(void const* data, unsigned int size) {
  concurrency::locker lock(&_lock_process);
  // Check viability.
  if (!_process)
    throw(basic_error() << "could not write on "
                           "process' input: process is not running");
  if (!_stream[in])
    throw(basic_error() << "could not write on process "
                        << _process->dwProcessId << "'s input: not connected");

  // Write data.
  DWORD wb;
  bool success(WriteFile(_stream[in], data, size, &wb, NULL) != 0);
  if (!success) {
    int errcode(GetLastError());
    throw(basic_error() << "could not write on process "
                        << _process->dwProcessId << "'s input (error "
                        << errcode << ")");
  }
  return (wb);
}

/**************************************
*                                     *
*           Private Methods           *
*                                     *
**************************************/

/**
 *  close syscall wrapper.
 *
 *  @param[in, out] fd The file descriptor to close.
 */
void process::_close(HANDLE& fd) throw() {
  if (!fd) {
    CloseHandle(fd);
    fd = NULL;
  }
  return;
}

/**
 *  Get is the current process run.
 *
 *  @return True is process run, otherwise false.
 */
bool process::_is_running() const throw() {
  return (_process || _stream[in] || _stream[out] || _stream[err]);
}

/**
 *  Create a pipe.
 *
 *  @param[out] rh Read end of the pipe.
 *  @param[out] wh Write end of the pipe.
 */
void process::_pipe(HANDLE* rh, HANDLE* wh) {
  if (!CreatePipe(rh, wh, NULL, 0)) {
    int errcode(GetLastError());
    throw(basic_error() << "pipe creation failed (error " << errcode << ")");
  }
  return;
}

/**
 *  Read data from HANDLE.
 *
 *  @param[in]  h    Handle
 *  @param[out] data Destination buffer.
 *  @param[in]  size Maximum number of bytes to read.
 *
 *  @return Number of bytes actually read.
 */
unsigned int process::_read(HANDLE h, void* data, unsigned int size) {
  if (!h)
    throw(basic_error() << "attempt to read from NULL handle");
  DWORD rb;
  bool success(ReadFile(h, data, size, &rb, NULL) != 0);
  if (!success) {
    int errcode(GetLastError());
    throw(basic_error() << "could not read from process (error " << errcode
                        << ")");
  }
  return (rb);
}

/**
 *  Callback to terminate window.
 *
 *  @param[in] hwnd    The window handle.
 *  @param[in] proc_id The current process id.
 *
 *  @return Always true.
 */
BOOL process::_terminate_window(HWND hwnd, LPARAM proc_id) {
  DWORD curr_proc_id(0);
  GetWindowThreadProcessId(hwnd, &curr_proc_id);
  if (curr_proc_id == static_cast<DWORD>(proc_id))
    PostMessage(hwnd, WM_CLOSE, 0, 0);
  return (TRUE);
}

/**
 *  Wait for process termination.
 *
 *  @param[in] timeout Maximum number of milliseconds to wait.
 *
 *  @return true if process exited.
 */
bool process::_wait(DWORD timeout) {
  if (!_process)
    throw(basic_error() << "could not wait on non-running process");
  DWORD ret(WaitForSingleObject(_process->hProcess, timeout));
  bool success(ret == WAIT_OBJECT_0);
  if (!success && (ret != WAIT_TIMEOUT)) {
    int errcode(GetLastError());
    throw(basic_error() << "could not wait process " << _process->dwProcessId
                        << " (error " << errcode << ")");
  }
  if (success) {
    _exit_code = EXIT_FAILURE;
    GetExitCodeProcess(_process->hProcess, &_exit_code);
    delete _process;
    _process = NULL;
    _close(_stream[in]);
  }
  return (success);
}
