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
#include "com/centreon/exceptions/basic.hh"
#include "com/centreon/concurrency/thread_pool.hh"

using namespace com::centreon::concurrency;

/**
 *  Check the thread pool max thread count.
 *
 *  @return 0 on success.
 */
int main() {
  try {
    thread_pool pool(4);
    if (pool.get_max_thread_count() != 4)
      throw (basic_error() << "get_max_thread_count failed");
    pool.set_max_thread_count(1);
    if (pool.get_max_thread_count() != 1)
      throw (basic_error() << "set_max_thread_count failed");
    pool.set_max_thread_count(0);
    if (pool.get_max_thread_count() <= 0)
      throw (basic_error() << "set_max_thread_count failed");
  }
  catch (std::exception const& e) {
    std::cerr << "error: " << e.what() << std::endl;
    return (1);
  }
  return (0);
}
