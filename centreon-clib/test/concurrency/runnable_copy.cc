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

#include <iostream>
#include "com/centreon/exception/basic.hh"
#include "com/centreon/concurrency/runnable.hh"

using namespace com::centreon::concurrency;

/**
 *  @class task
 *  @brief litle implementation of runnable to test the thread pool.
 */
class  task : public runnable {
public:
       task(bool auto_delete) {
         set_auto_delete(auto_delete);
       }
       ~task() throw () {}
  void run() {}
};

/**
 *  Check the runnable copy.
 *
 *  @return 0 on success.
 */
int main() {
  try {
    task ref(true);

    task t1(ref);
    if (ref.get_auto_delete() != t1.get_auto_delete())
      throw (basic_error() << "invalid copy constructor");

    task t2 = ref;
    if (ref.get_auto_delete() != t2.get_auto_delete())
      throw (basic_error() << "invalid copy operator");
  }
  catch (std::exception const& e) {
    std::cerr << "error: " << e.what() << std::endl;
    return (1);
  }
  return (0);
}
