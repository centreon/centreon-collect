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

#ifndef CC_CONCURRENCY_LOCKER_HH
#  define CC_CONCURRENCY_LOCKER_HH

#  include "com/centreon/namespace.hh"
#  include "com/centreon/concurrency/mutex.hh"

CC_BEGIN()

namespace   concurrency {
  /**
   *  @class locker locker.hh "com/centreon/concurrency/locker.hh"
   *  @brief Provide a simple way to lock ans un lock mutex.
   *
   *  Allow simple method to lock and unlock mutex.
   */
  class     locker {
  public:
            locker(mutex* m = NULL);
            ~locker() throw ();
    mutex*  get_mutex() const throw();
    void    relock();
    void    unlock();

  private:
            locker(locker const& right);
    locker& operator=(locker const& right);
    locker& _internal_copy(locker const& right);

    bool   _is_lock;
    mutex* _m;
  };
}

CC_END()

#endif // !CC_CONCURRENCY_LOCKER_HH
