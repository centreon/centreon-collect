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

#ifndef CC_TEST_CONCURRENCY_THREAD_TEST_HH
#  define CC_TEST_CONCURRENCY_THREAD_TEST_HH

#  include "com/centreon/concurrency/locker.hh"

CC_BEGIN()

namespace concurrency {
  /**
   *  @class thread_test
   *  @brief litle implementation of thread to test concurrency thread.
   */
  class   thread_test : public thread {
  public:
          thread_test() : _quit(false) {}
          ~thread_test() throw () {}
    void  quit() { _quit = true; }

  private:
    void  _run() { while (!_quit) yield(); }
    bool  _quit;
  };
}

CC_END()

#endif // !CC_TEST_CONCURRENCY_THREAD_TEST_HH
