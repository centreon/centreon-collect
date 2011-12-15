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

#include <exception>
#include <iostream>
#include "com/centreon/exception/basic.hh"
#include "com/centreon/concurrency/mutex.hh"
#include "com/centreon/concurrency/locker.hh"

using namespace com::centreon::concurrency;

/**
 *  Check the locker copy.
 *
 *  @return 0 on success.
 */
int main() {
  try {
    mutex mtx;
    locker lock1(&mtx);

    locker lock2(lock1);
    if (lock1.get_mutex() != lock2.get_mutex())
      throw (basic_error() << "copy constructor failed");

    locker lock3 = lock1;
    if (lock1.get_mutex() != lock3.get_mutex())
      throw (basic_error() << "copy operator failed");
  }
  catch (std::exception const& e) {
    std::cerr << "error: " << e.what() << std::endl;
    return (1);
  }
  return (0);
}
