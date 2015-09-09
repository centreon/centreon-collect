/*
** Copyright 2011-2013 Centreon
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
**
** For more information : contact@centreon.com
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

    io::file_stream fs1(stdin);
    hm.add(&fs1, &l);
    if (hm.remove(&l) != 1)
      throw (basic_error() << "remove one listener failed");

    hm.add(&fs1, &l);
    io::file_stream fs2(stdout);
    hm.add(&fs2, &l);
    io::file_stream fs3(stderr);
    hm.add(&fs3, &l);

    if (hm.remove(&l) != 3)
      throw (basic_error() << "remove three listener failed");
  }
  catch (std::exception const& e) {
    std::cerr << "error: " << e.what() << std::endl;
    return (1);
  }
  return (0);
}
