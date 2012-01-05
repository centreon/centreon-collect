/*
** Copyright 2011-2012 Merethis
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

#ifndef CC_HANDLE_MANAGER_POSIX_HH
#  define CC_HANDLE_MANAGER_POSIX_HH

#  include <map>
#  include <utility>
#  include <poll.h>
#  include "com/centreon/namespace.hh"
#  include "com/centreon/handle.hh"
#  include "com/centreon/handle_listener.hh"
#  include "com/centreon/task_manager.hh"
#  include "com/centreon/task.hh"

CC_BEGIN()

/**
 *  @class handle_manager handle_manager.hh "com/centreon/handle_manager.hh"
 *  @brief Provide handle manager.
 *
 *  Allow to manage handle, and notify by listener when some thing needs
 *  to be done.
 */
class                handle_manager {
public:
                     handle_manager(task_manager* tm = NULL);
  virtual            ~handle_manager() throw ();
  void               add(
                       handle* h,
                       handle_listener* hl,
                       bool is_runnable = false);
  void               link(task_manager* tm);
  void               multiplex();
  bool               remove(handle* h);
  unsigned int       remove(handle_listener* hl);

private:
  class              internal_task : public task {
  public:
    enum             action {
      none = 0,
      read = 1,
      write = 2,
      error = 4,
      close = 8
    };

                     internal_task(
                       handle* h,
                       handle_listener* hl,
                       bool is_runnable);
                     ~internal_task() throw ();
    void             add_action(action a) throw ();
    bool             is_runnable() const throw ();
    handle*          get_handle() const throw ();
    handle_listener* get_handle_listener() const throw ();
    void             run();

  private:
                     internal_task(internal_task const& right);
    internal_task&   operator=(internal_task const& right);
    internal_task&   _internal_copy(internal_task const& right);

    unsigned int     _action;
    bool             _is_runnable;
    handle*          _h;
    handle_listener* _hl;
  };

                     handle_manager(handle_manager const& right);
  handle_manager&    operator=(handle_manager const& right);
  void               _create_fds();
  handle_manager&    _internal_copy(handle_manager const& right);
  static int         _poll(
                       pollfd *fds,
                       nfds_t nfds,
                       int timeout) throw ();

  pollfd*            _fds;
  std::map<native_handle, internal_task*>
                     _handles;
  bool               _should_create_fds;
  task_manager*      _task_manager;
};

CC_END()

#endif // !CC_HANDLE_MANAGER_POSIX_HH
