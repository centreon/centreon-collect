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

#ifndef CC_PROCESS_POSIX_HH
#  define CC_PROCESS_POSIX_HH

#  include <string>
#  include <sys/types.h>
#  include "com/centreon/namespace.hh"

CC_BEGIN()

/**
 *  @class process process_posix.hh "com/centreon/process_posix.hh"
 *  @brief Process execution class.
 *
 *  Execute external process.
 */
class                process {
public:
  enum               status {
    normal = 0,
    crash = 1
  };
  enum               stream {
    in = 0,
    out = 1,
    err = 2
  };

                     process();
  virtual            ~process() throw ();
  void               enable_stream(stream s, bool enable);
  void               exec(char const* cmd, char** env = NULL);
  void               exec(std::string const& cmd);
  int                exit_code() const throw ();
  status             exit_status() const throw ();
  void               kill();
  unsigned int       read(void* data, unsigned int size);
  unsigned int       read_err(void* data, unsigned int size);
  void               terminate();
  void               wait();
  bool               wait(unsigned long timeout);
  unsigned int       write(std::string const& data);
  unsigned int       write(void const* data, unsigned int size);

private:
                     process(process const& p);
  process&           operator=(process const& p);
  static void        _close(int& fd) throw ();
  static void        _dev_null(int fd, int flags);
  static int         _dup(int oldfd);
  static void        _dup2(int oldfd, int newfd);
  void               _internal_copy(process const& p);
  void               _kill(int sig);
  static void        _pipe(int fds[2]);
  unsigned int       _read(int fd, void* data, unsigned int size);
  static void        _set_cloexec(int fd);

  bool               _enable_stream[3];
  pid_t              _process;
  int                _status;
  int                _stream[3];
};

CC_END()

#endif // !CC_PROCESS_POSIX_HH
