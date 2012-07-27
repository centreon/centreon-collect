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

#include <cassert.h>
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
process::process()
  : _exit_code(0),
    _process(NULL) {
  memset(_enable_stream, 1, sizeof(_enable_stream));
  memset(_stream, NULL, sizeof(_stream));
}

/**
 *  Destructor.
 */
process::~process() throw () {
  for (unsigned int i(0); i < 3; ++i)
    _close(_stream[i]);
}

/**
 *  Enable or disable process' stream.
 *
 *  @param[in] s      The stream to set.
 *  @param[in] enable Set to true to enable stderr.
 */
void process::enable_stream(stream s, bool enable) {
  if (_enable_stream[s] != enable) {
    if (!_process)
      _enable_stream[s] = enable;
    else if (!enable)
      _close(_stream[s]);
    else
      throw (basic_error() << "cannot reenable \""
             << s << "\" while process is running");
  }
  return;
}

/**
 *  Run process.
 *
 *  @param[in] cmd Command line.
 *  @param[in] env Array of strings (on form key=value), which are
 *                 passed as environment to the new process. If env
 *                 is NULL, the current environement are passed of
 *                 the new process.
 */
void process::exec(char const* cmd, char** env) {
  // Check viability.
  if (_process)
    throw (basic_error() << "process " << _process->dwProcessId
           << " is already started and has not been waited");

  // Reset exit code.
  _exit_code = 0;

  HANDLE child_stream[3] = { NULL, NULL, NULL };

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
    unsigned int size(cmd.size() + 1);
    unique_array_ptr<char> cmd_str(new char[size]);
    memcpy(cmd_str.get(), cmd.c_str(), size);
    if (CreateProcess(
          NULL,
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
      throw (basic_error() << "could not create process (error "
             << errcode << ")");
    }

    for (unsigned int i(0); i < 3; ++i)
      _close(child_stream[i]);
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
 *  @param[in] cmd Command line.
 */
void process::exec(std::string const& cmd) {
  exec(cmd.c_str());
  return;
}

/**
 *  Get the exit code, return by the executed process.
 *
 *  @return The exit code.
 */
int process::exit_code() const throw () {
  return (_exit_code);
}

/**
 *  Get the exit status, always return normal into windows.
 *
 *  @return The exit status.
 */
process::status process::exit_status() const throw () {
  return (process::normal);
}

/**
 *  Kill process.
 */
void process::kill() {
  if (_process) {
    if (!TerminateProcess(_process->hProcess, EXIT_FAILURE)) {
      int errcode(GetLastError());
      throw (basic_error() << "could not terminate process "
             << _process->dwProcessId << " (error " << errcode << ")");
    }
  }
}

/**
 *  Read data from stdout.
 *
 *  @param[in] data Destination buffer.
 *  @param[in] size Maximum number of bytes to read.
 *
 *  @return Number of bytes actually read.
 */
unsigned int process::read(void* data, unsigned int size) {
  return (_read(_out, data, size));
}

/**
 *  Read data from stderr.
 *
 *  @param[in] data Destination buffer.
 *  @param[in] size Maximum number of bytes to read.
 *
 *  @return Number of bytes actually read.
 */
unsigned int process::read_err(void* data, unsigned int size) {
  return (_read(_err, data, size));
}

/**
 *  Terminate the process.
 */
void process::terminate() {
  if (_process) {
    EnumWindows(
      &_terminate_window,
      static_cast<LPARAM>(_process->dwProcessId));
    PostThreadMessage(_process->dwThreadId, WM_CLOSE, 0, 0);
  }
  return;
}

/**
 *  Wait for process termination.
 */
void process::wait() {
  _wait(INFINITE);
}

/**
 *  Wait for process termination.
 *
 *  @param[in]  timeout   Maximum number of milliseconds to wait for
 *                        process termination.
 *  @return true if process exited.
 */
bool process::wait(unsigned long timeout) {
  return (_wait(timeout));
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
  // Check viability.
  if (!_process)
    throw (basic_error() << "could not write on " \
           "process' input: process is not running");
  if (!_stream[in])
    throw (basic_error() << "could not write on process "
           << _process->dwProcessId << "'s input: not connected");

  // Write data.
  DWORD wb;
  bool success(WriteFile(_stream[in], data, size, &wb, NULL) != 0);
  if (!success) {
    int errcode(GetLastError());
    throw (basic_error() << "could not write on process "
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
 *  Copy constructor.
 *
 *  @param[in] p Object to copy.
 */
process::process(process const& p) {
  _internal_copy(p);
}

/**
 *  Assignment operator.
 *
 *  @param[in] p Object to copy.
 *
 *  @return This object.
 */
process& process::operator=(process const& p) {
  _internal_copy(p);
  return (*this);
}

/**
 *  close syscall wrapper.
 *
 *  @param[in, out] fd The file descriptor to close.
 */
void process::_close(HANDLE& fd) throw () {
  if (!fd) {
    CloseHandle(fd);
    fd = NULL;
  }
  return;
}

/**
 *  Copy internal data members.
 *
 *  @param[in] p Object to copy.
 */
void process::_internal_copy(process const& p) {
  (void)p;
  assert(!"process is not copyable");
  abort();
  return;
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
    throw (basic_error() << "pipe creation failed (error "
           << errcode << ")");
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
    throw (basic_error() << "attempt to read from NULL handle");
  DWORD rb;
  bool success(ReadFile(h, data, size, &rb, NULL) != 0);
  if (!success) {
    int errcode(GetLastError());
    throw (basic_error() << "could not read from process (error "
           << errcode << ")");
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
    throw (basic_error() << "could not wait on non-running process");
  DWORD ret(WaitForSingleObject(_process->hProcess, timeout));
  bool success(ret == WAIT_OBJECT_0);
  if (!success && (ret != WAIT_TIMEOUT)) {
    int errcode(GetLastError());
    throw (basic_error() << "could not wait process "
           << _process->dwProcessId << " (error " << errcode << ")");
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
