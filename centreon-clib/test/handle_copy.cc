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
#include "com/centreon/handle_manager.hh"

using namespace com::centreon;

/**
 *  @class standard
 *  @brief litle implementation of handle to test handle copy.
 */
class           standard : public handle {
public:
                standard(native_handle internal) : handle(internal){}
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
 *  Check handle copy.
 *
 *  @return 0 on success.
 */
int main() {
  try {
    standard ref(42);

    standard s1(ref);
    if (ref.get_internal_handle() != s1.get_internal_handle())
      throw (basic_error() << "invalid copy constructor");

    standard s2 = ref;
    if (ref.get_internal_handle() != s2.get_internal_handle())
      throw (basic_error() << "invalid copy operator");
  }
  catch (std::exception const& e) {
    std::cerr << "error: " << e.what() << std::endl;
    return (1);
  }
  return (0);
}
