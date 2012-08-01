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

#ifndef CC_PROCESS_MANAGER_POSIX_HH
#  define CC_PROCESS_MANAGER_POSIX_HH

#  include <poll.h>
#  include "com/centreon/concurrency/mutex.hh"
#  include "com/centreon/concurrency/thread.hh"
#  include "com/centreon/namespace.hh"
#  include "com/centreon/unordered_hash.hh"

CC_BEGIN()

class process;
class process_listener;

/**
 *  @class process_manager process_manager_posix.hh "com/centreon/process_manager_posix.hh"
 *  @brief This class manage process.
 *
 *  This class is a singleton and manage process.
 */
class                     process_manager
  : public concurrency::thread {
public:
  void                    add(process* p);
  static process_manager& instance();
  static void             load();
  static void             unload();

private:
                          process_manager();
                          process_manager(process_manager const& p);
                          ~process_manager() throw ();
  process_manager&        operator=(process_manager const& p);
  void                    _close_stream(int fd) throw ();
  void                    _erase_timeout(process* p);
  void                    _internal_copy(process_manager const& p);
  void                    _kill_processes_timeout() throw ();
  void                    _read_stream(int fd) throw ();
  void                    _run();
  void                    _update_list();
  void                    _wait_processes() throw ();

  pollfd*                 _fds;
  unsigned int            _fds_capacity;
  unsigned int            _fds_size;
  concurrency::mutex      _lock_processes;
  umap<int, process*>     _processes_fd;
  umap<pid_t, process*>   _processes_pid;
  umultimap<unsigned int, process*>
                          _processes_timeout;
  bool                    _quit;
  bool                    _update;
};

CC_END()

#endif // !CC_PROCESS_MANAGER_POSIX_HH
