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
 *  Check the timestamp constructor.
 *
 *  @return 0 on success.
 */
int main() {
  try {
    timestamp t1;
    if (t1.to_mseconds() || t1.to_seconds() || t1.to_useconds())
      throw(basic_error() << "default constructor failed");

    timestamp t2(42);
    if (t2.to_seconds() != 42)
      throw(basic_error() << "constructor failed");

    timestamp t3(42, 24);
    if (t3.to_useconds() != 42000024)
      throw(basic_error() << "constructor failed");
  }
  catch (std::exception const& e) {
    std::cerr << "error: " << e.what() << std::endl;
    return (1);
  }
  return (0);
}
