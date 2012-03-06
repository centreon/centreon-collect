/*
** Copyright 2011-2012 Merethis
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
#include "com/centreon/exceptions/basic.hh"
#include "com/centreon/shared_ptr.hh"

using namespace com::centreon;

/**
 *  Check the shared_ptr clear.
 *
 *  @return 0 on success.
 */
int main() {
  int ret(0);
  try {
    shared_ptr<int> ptr(NULL);
    ptr.clear();
    if (ptr.get())
      throw (basic_error() << "clear null pointer failed");
    ptr = shared_ptr<int>(new int(42));
    ptr.clear();
    if (ptr.get())
      throw (basic_error() << "clear pointer failed");
    shared_ptr<int> ptr2(new int(42));
    ptr = ptr2;
    ptr2.clear();
    if (!ptr.get())
      throw (basic_error() << "clear failed with multiple reference");
  }
  catch (std::exception const& e) {
    ret = 1;
    std::cerr << "error: " << e.what() << std::endl;
  }
  return (ret);
}
