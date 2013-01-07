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

#ifndef CC_CONCURRENCY_READ_WRITE_LOCK_POSIX_HH
#  define CC_CONCURRENCY_READ_WRITE_LOCK_POSIX_HH

#  include <pthread.h>
#  include "com/centreon/namespace.hh"

CC_BEGIN()

namespace            concurrency {
  /**
   *  @class read_write_lock read_write_lock_posix.hh "com/centreon/concurrency/read_write_lock.hh"
   *  @brief Readers/writer lock.
   *
   *  Implementation of the readers/writer lock synchronization
   *  primitive.
   */
  class              read_write_lock {
  public:
                     read_write_lock();
                     ~read_write_lock() throw ();
    void             read_lock();
    bool             read_lock(unsigned long timeout);
    bool             read_trylock();
    void             read_unlock();
    void             write_lock();
    bool             write_lock(unsigned long timeout);
    bool             write_trylock();
    void             write_unlock();

  private:
                     read_write_lock(read_write_lock const& right);
    read_write_lock& operator=(read_write_lock const& right);
    void             _internal_copy(read_write_lock const& right);

    pthread_rwlock_t _rwl;
  };
}

CC_END()

#endif // !CC_CONCURRENCY_READ_WRITE_LOCK_POSIX_HH
