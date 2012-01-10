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
#include <stdio.h>
#include "com/centreon/exceptions/basic.hh"
#include "com/centreon/handle_listener.hh"
#include "com/centreon/handle_manager.hh"
#include "com/centreon/io/file_stream.hh"

using namespace com::centreon;

/**
 *  @class listener
 *  @brief litle implementation of handle listener to test the
 *         handle manager.
 */
class     listener : public handle_listener {
public:
          listener() {}
          ~listener() throw () {}
  void    error(handle& h) { (void)h; }
};

/**
 *  Check the handle manager remove by handle.
 *
 *  @return 0 on success.
 */
int main() {
  try {
    handle_manager hm;
    if (hm.remove(static_cast<handle*>(NULL)))
      throw (basic_error() << "remove null pointer");

    io::file_stream fs(stdin);
    if (hm.remove(&fs))
      throw (basic_error() << "remove invalid handle");

    listener l;
    hm.add(&fs, &l);
    if (!hm.remove(&fs))
      throw (basic_error() << "remove failed");
  }
  catch (std::exception const& e) {
    std::cerr << "error: " << e.what() << std::endl;
    return (1);
  }
  return (0);
}
