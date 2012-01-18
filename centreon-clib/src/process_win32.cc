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
#include <string.h>
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
  : _err(NULL),
    _in(NULL),
    _out(NULL),
    _process(NULL),
    _with_err(false),
    _with_in(false),
    _with_out(false) {}

/**
 *  Destructor.
 */
process::~process() throw () {
  _terminated();
}

/**
 *  Run process.
 *
 *  @param[in] cmd Command line.
 */
void process::exec(std::string const& cmd) {
  // Check viability.
  if (_process)
    throw (basic_error() << "process " << GetProcessId(_process)
           << " is already started and has not been waited");

  // Create pipes if necessary.
  HANDLE child_err(NULL);
  HANDLE child_in(NULL);
  HANDLE child_out(NULL);
  try {
    if (_with_err) {
      _pipe(&_err, &child_err);
      SetHandleInformation(_err, HANDLE_FLAG_INHERIT, 0);
    }
    if (_with_in) {
      _pipe(&child_in, &_in);
      SetHandleInformation(_in, HANDLE_FLAG_INHERIT, 0);
    }
    if (_with_out) {
      _pipe(&_out, &child_out);
      SetHandleInformation(_out, HANDLE_FLAG_INHERIT, 0);
    }
  }
  catch (...) {
    if (child_err) {
      CloseHandle(child_err);
      CloseHandle(_err);
      _err = NULL;
    }
    if (child_in) {
      CloseHandle(child_in);
      CloseHandle(_in);
      _in = NULL;
    }
    throw ;
  }

  // Startup informations.
  STARTUPINFO si;
  memset(&si, 0, sizeof(si));
  si.cb = sizeof(si);
  si.hStdError = child_err;
  si.hStdInput = child_in;
  si.hStdOutput = child_out;
  si.dwFlags = STARTF_USESTDHANDLES;

  // Create process.
  bool success(false);
  exceptions::basic error = basic_error();
  try {
    PROCESS_INFORMATION pi;
    memset(&pi, 0, sizeof(pi));
    size_t size(cmd.size() + 1);
    unique_array_ptr cmdstr(new char[size]);
    memcpy(cmdstr.get(), cmd.c_str(), size);
    success = (CreateProcess(
                 NULL,
                 cmdstr.get(),
                 NULL,
                 NULL,
                 TRUE,
                 0,
                 NULL,
                 NULL,
                 &si,
                 &pi) != FALSE);
    if (!success) {
      int errcode(GetLastError());
      error << "could not create process (error " << errcode << ")";
    }
    else
      _process = pi.hProcess;
  }
  catch (std::exception const& e) {
    error << "could not create process: " << e.what();
    _terminated();
  }
  catch (...) {
    error << "could not create process: unknown error";
    _terminated();
  }

  // Close handles.
  if (child_err)
    CloseHandle(child_err);
  if (child_in)
    CloseHandle(child_in);
  if (child_out)
    CloseHandle(child_out);

  // Throw exception if error occurred.
  if (!success)
    throw (error);

  return ;
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
    if (!TerminateProcess(_process, EXIT_FAILURE)) {
      int errcode(GetLastError());
      throw (basic_error() << "could not terminate process "
             << GetProcessId(_process) << " (error " << errcode << ")");
    }
  }
  return ;
}

/**
 *  Wait for process termination.
 *
 *  @return Process exit code.
 */
int process::wait() {
  int exit_code;
  _wait(INFINITE, &exit_code);
  return (exit_code);
}

/**
 *  Wait for process termination.
 *
 *  @param[in]  timeout   Maximum number of milliseconds to wait for
 *                        process termination.
 *  @param[out] exit_code Will be set to the process' exit code.
 *
 *  @return true if process exited.
 */
bool process::wait(unsigned long timeout, int* exit_code) {
  return (_wait(timeout, exit_code));
}

/**
 *  Enable or disable process' stderr.
 *
 *  @param[in] enable Set to true to enable stderr.
 */
void process::with_stderr(bool enable) {
  _with_std(enable, &_with_err, &_err, "stderr");
  return ;
}

/**
 *  Enable or disable process' stdin.
 *
 *  @param[in] enable Set to true to enable stdin.
 */
void process::with_stdin(bool enable) {
  _with_std(enable, &_with_in, &_in, "stdin");
  return ;
}

/**
 *  Enable or disable process' stdout.
 *
 *  @param[in] enable Set to true to enable stdout.
 */
void process::with_stdout(bool enable) {
  _with_std(enable, &_with_out, &_out, "stdout");
  return ;
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
  if (!_in)
    throw (basic_error() << "could not write on process "
           << GetProcessId(_process) << "'s input: not connected");

  // Write data.
  DWORD wb;
  bool success(WriteFile(_in, data, size, &wb, NULL) != 0);
  if (!success) {
    int errcode(GetLastError());
    throw (basic_error() << "could not write on process "
           << GetProcessId(_process) << "'s input (error "
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
 *  Copy internal data members.
 *
 *  @param[in] p Object to copy.
 */
void process::_internal_copy(process const& p) {
  (void)p;
  assert(!"process is not copyable");
  abort();
  return ;
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
  return ;
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
 *  Reset fields of process after execution.
 */
void process::_terminated() throw () {
  if (_process) {
    CloseHandle(_process);
    _process = NULL;
  }
  if (_err) {
    CloseHandle(_err);
    _err = NULL;
  }
  if (_in) {
    CloseHandle(_in);
    _in = NULL;
  }
  if (_out) {
    CloseHandle(_out);
    _out = NULL;
  }
  return ;
}

/**
 *  Wait for process termination.
 *
 *  @param[in]  timeout   Maximum number of milliseconds to wait.
 *  @param[out] exit_code Process' exit code.
 *
 *  @return true if process exited.
 */
bool process::_wait(DWORD timeout, int* exit_code) {
  if (!_process)
    throw (basic_error() << "could not wait on non-running process");
  DWORD ret(WaitForSingleObject(_process, timeout));
  bool success(ret == WAIT_OBJECT_0);
  if (!success && (ret != WAIT_TIMEOUT)) {
    int errcode(GetLastError());
    throw (basic_error() << "could not wait process "
           << GetProcessId(_process) << " (error " << errcode << ")");
  }
  if (success) {
    if (exit_code) {
      DWORD ec(EXIT_FAILURE);
      GetExitCodeProcess(_process, &ec);
      *exit_code = ec;
    }
    _terminated();
  }
  return (success);
}

/**
 *  Enable or disable standard process objects.
 *
 *  @param[in]     enable New boolean flag.
 *  @param[in,out] b      Boolean flag.
 *  @param[in,out] h      Handle.
 *  @param[in]     name   Object name (for error reporting).
 */
void process::_with_std(
                bool enable,
                bool* b,
                HANDLE* h,
                char const* name) {
  if (*b != enable) {
    if (!_process)
      *b = enable;
    else if (!enable) {
      if (*h) {
        CloseHandle(*h);
        *h = NULL;
      }
    }
    else
      throw (basic_error() << "cannot reenable "
             << name << " while process is running");
  }
  return ;
}
