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
 *  Check the shared_ptr copy.
 *
 *  @return 0 on success.
 */
int main() {
  int ret(0);
  try {
    shared_ptr<int> ref(new int(1));
    shared_ptr<int> c1(ref);
    if (c1.get() != ref.get())
      throw (basic_error() << "copy constructor failed");
    shared_ptr<int> c2 = ref;
    if (c2.get() != ref.get())
      throw (basic_error() << "assigmente operator failed");
  }
  catch (std::exception const& e) {
    ret = 1;
    std::cerr << "error: " << e.what() << std::endl;
  }
  return (ret);
}
