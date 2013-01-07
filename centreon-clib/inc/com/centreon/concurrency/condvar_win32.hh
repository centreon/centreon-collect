/*
** Copyright 2012-2013 Merethis
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

#ifndef CC_CONCURRENCY_CONDVAR_WIN32_HH
#  define CC_CONCURRENCY_CONDVAR_WIN32_HH

#  include <windows.h>
#  include "com/centreon/namespace.hh"

CC_BEGIN()

namespace              concurrency {
  // Forward declaration.
  class                mutex;

  /**
   *  @class condvar condvar_win32.hh "com/centreon/concurrency/condvar.hh"
   *  @brief Simple thread synchronization.
   *
   *  Provide condition variable for synchronization between threads.
   */
  class                condvar {
  public:
                       condvar();
                       ~condvar() throw ();
    void               wait(mutex* mutx);
    bool               wait(mutex* mutx, unsigned long timeout);
    void               wake_all();
    void               wake_one();

  private:
                       condvar(condvar const& cv);
    condvar&           operator=(condvar const& cv);
    void               _internal_copy(condvar const& cv);
    bool               _wait(mutex* mutx, DWORD timeout);

    CONDITION_VARIABLE _cond;
  };
}

CC_END()

#endif // !CC_CONCURRENCY_CONDVAR_WIN32_HH
