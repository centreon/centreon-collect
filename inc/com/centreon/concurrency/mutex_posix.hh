/*
** Copyright 2011 Merethis
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

#ifndef CC_CONCURRENCY_MUTEX_POSIX_HH
#  define CC_CONCURRENCY_MUTEX_POSIX_HH

#  include <pthread.h>
#  include "com/centreon/namespace.hh"

CC_BEGIN()

namespace           concurrency {
  /**
   *  @class mutex mutex_posix.hh "com/centreon/concurrency/mutex.hh"
   *  @brief Implements a mutex.
   *
   *  POSIX-based implementation of a mutex.
   */
  class             mutex {
    friend class    wait_condition;
  public:
                    mutex();
                    ~mutex() throw ();
    void            lock();
    bool            trylock();
    void            unlock();

  private:
                    mutex(mutex const& right);
    mutex&          operator=(mutex const& right);
    mutex&          _internal_copy(mutex const& right);

    pthread_mutex_t _mtx;
  };
}

CC_END()

#endif // !CC_CONCURRENCY_MUTEX_POSIX_HH
