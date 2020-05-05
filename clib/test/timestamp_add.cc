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
#include "com/centreon/exceptions/basic.hh"
#include "com/centreon/timestamp.hh"

using namespace com::centreon;

/**
 *  Check the timestamp add.
 *
 *  @return 0 on success.
 */
int main() {
  try {
    timestamp t1(1, 42);
    timestamp t2(2, 24);

    timestamp t3(t1 + t2);
    if (t3.to_useconds() != 3000066)
      throw(basic_error() << "operator+ failed");

    timestamp t4(-1, -24);
    timestamp t5(t2 + t4);
    if (t5.to_useconds() != 1000000)
      throw(basic_error() << "operator+ failed");

    timestamp t6(1, 42);
    t6 += t2;
    if (t6.to_useconds() != 3000066)
      throw(basic_error() << "operator+= failed");

    timestamp t7(2, 24);
    t7 += t4;
    if (t7.to_useconds() != 1000000)
      throw(basic_error() << "operator+= failed");
  }
  catch (std::exception const& e) {
    std::cerr << "error: " << e.what() << std::endl;
    return (1);
  }
  return (0);
}
