/*
** Copyright 2012 Merethis
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

#include <cstdlib>
#include "com/centreon/concurrency/mutex.hh"

using namespace com::centreon;

#define RECURSIVE_LEVEL 1000

/**
 *  Check that mutex can be recursively locked.
 *
 *  @return EXIT_SUCCESS on success.
 */
int main() {
  // Mutex.
  concurrency::mutex mtx;

  // Recursively lock mutex.
  for (unsigned int i(0); i < RECURSIVE_LEVEL; ++i)
    mtx.lock();

  // Recursively unlock mutex.
  for (unsigned int i(0); i < RECURSIVE_LEVEL; ++i)
    mtx.unlock();

  return (EXIT_SUCCESS);
}
