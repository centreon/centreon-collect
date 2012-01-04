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

#ifndef CC_CONCURRENCY_WAIT_CONDITION_POSIX_HH
#  define CC_CONCURRENCY_WAIT_CONDITION_POSIX_HH

#  include <pthread.h>
#  include <limits.h>
#  include "com/centreon/namespace.hh"
#  include "com/centreon/concurrency/mutex_posix.hh"

CC_BEGIN()

namespace           concurrency {
  /**
   *  @class wait_condition wait_condition_posix.hh "com/centreon/concurrency/wait_condition.hh"
   *  @brief Allow simple threads synchronization.
   *
   *  Provide condition variable for synchronization threads.
   */
  class             wait_condition {
  public:
                    wait_condition();
                    ~wait_condition() throw ();
    void            wait(mutex* mtx);
    bool            wait(mutex* mtx, unsigned long timeout);
    void            wake_all();
    void            wake_one();

  private:
                    wait_condition(wait_condition const& right);
    wait_condition& operator=(wait_condition const& right);
    wait_condition& _internal_copy(wait_condition const& right);

    pthread_cond_t  _cnd;
  };
}

CC_END()

#endif // !CC_CONCURRENCY_WAIT_CONDITION_POSIX_HH
