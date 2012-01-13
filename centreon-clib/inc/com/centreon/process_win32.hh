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

#ifndef CC_PROCESS_WIN32_HH
#  define CC_PROCESS_WIN32_HH

#  include <string>
#  include <windows.h>
#  include "com/centreon/namespace.hh"

CC_BEGIN()

/**
 *  @class process process_win32.hh "com/centreon/process.hh"
 *  @brief Process execution class.
 *
 *  Execute external process.
 */
class                process {
public:
                     process();
  virtual            ~process() throw ();
  void               exec(std::string const& cmd);
  unsigned int       read(void* data, unsigned int size);
  unsigned int       read_err(void* data, unsigned int size);
  void               terminate();
  int                wait();
  bool               wait(unsigned long timeout, int* exit_code = NULL);
  void               with_stderr(bool enable);
  void               with_stdin(bool enable);
  void               with_stdout(bool enable);
  unsigned int       write(void const* data, unsigned int size);

private:
                     process(process const& p);
  process&           operator=(process const& p);
  void               _internal_copy(process const& p);
  static void        _pipe(HANDLE* rh, HANDLE* wh);
  unsigned int       _read(HANDLE h, void* data, unsigned int size);
  void               _terminated() throw ();
  bool               _wait(DWORD timeout, int* exit_code);
  void               _with_std(
                       bool enable,
                       bool* b,
                       HANDLE* h,
                       char const* name);

  HANDLE             _err;
  HANDLE             _in;
  HANDLE             _out;
  HANDLE             _process;
  bool               _with_err;
  bool               _with_in;
  bool               _with_out;
};

CC_END()

#endif // !CC_PROCESS_WIN32_HH
