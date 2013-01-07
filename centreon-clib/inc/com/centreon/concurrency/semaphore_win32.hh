/*
** Copyright 2011-2013 Merethis
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

#ifndef CC_CONCURRENCY_SEMAPHORE_WIN32_HH
#  define CC_CONCURRENCY_SEMAPHORE_WIN32_HH

#  include <windows.h>
#  include "com/centreon/namespace.hh"

CC_BEGIN()

namespace      concurrency {
  /**
   *  @class semaphore semaphore_win32.hh "com/centreon/concurrency/semaphore.hh"
   *  @brief Implements a semaphore.
   *
   *  Win32 implementation of a semaphore.
   */
  class        semaphore {
  public:
               semaphore(unsigned int n = 0);
               ~semaphore() throw ();
    void       acquire();
    bool       acquire(unsigned long timeout);
    int        available();
    void       release();
    bool       try_acquire();

  private:
               semaphore(semaphore const& s);
    semaphore& operator=(semaphore const& s);
    void       _internal_copy(semaphore const& right);

    HANDLE     _sem;
  };
}

CC_END()

#endif // !CC_CONCURRENCY_SEMAPHORE_WIN32_HH
