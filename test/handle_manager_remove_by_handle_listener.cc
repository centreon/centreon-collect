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
#include "com/centreon/exceptions/basic.hh"
#include "com/centreon/handle_manager.hh"

using namespace com::centreon;

/**
 *  @class standard
 *  @brief litle implementation of handle to test the handle manager.
 */
class           standard : public handle {
public:
                standard() : handle() {
                  static int i(0);
                  _internal_handle = ++i;
                }
                ~standard() throw () {}
  void          close() {}
  unsigned long read(void* data, unsigned long size) {
    (void)data;
    (void)size;
    return (0);
  }
  unsigned long write(void const* data, unsigned long size) {
    (void)data;
    (void)size;
    return (0);
  }
};

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
 *  Check the handle manager remove by handle listener.
 *
 *  @return 0 on success.
 */
int main() {
  try {
    handle_manager hm;
    if (hm.remove(static_cast<handle_listener*>(NULL)))
      throw (basic_error() << "remove null pointer");

    listener l;
    if (hm.remove(&l))
      throw (basic_error() << "remove invalid listener");

    standard s1;
    hm.add(&s1, &l);
    if (hm.remove(&l) != 1)
      throw (basic_error() << "remove one listener failed");

    hm.add(&s1, &l);
    standard s2;
    hm.add(&s2, &l);
    standard s3;
    hm.add(&s3, &l);
    standard s4;
    hm.add(&s4, &l);

    if (hm.remove(&l) != 4)
      throw (basic_error() << "remove four listener failed");
  }
  catch (std::exception const& e) {
    std::cerr << "error: " << e.what() << std::endl;
    return (1);
  }
  return (0);
}
