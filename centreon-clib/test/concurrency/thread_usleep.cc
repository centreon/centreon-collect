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

#include <exception>
#include <iostream>
#include "com/centreon/concurrency/thread.hh"
#include "com/centreon/exceptions/basic.hh"
#include "com/centreon/timestamp.hh"

using namespace com::centreon;

/**
 *  Check the sleep methode.
 *
 *  @return 0 on success.
 */
int main() {
  try {
    unsigned long waiting(5000000l);
    timestamp start(timestamp::now());
    concurrency::thread::usleep(waiting);
    timestamp end(timestamp::now());
    timestamp diff(end - start);
    if (diff.to_useconds() > waiting * 1.20)
      throw(basic_error() << "waiting more than necessary: "
                          << diff.to_useconds() << "/" << waiting);
    if (diff.to_useconds() < waiting * 0.90)
      throw(basic_error() << "waiting less than necessary: "
                          << diff.to_useconds() << "/" << waiting);
  }
  catch (std::exception const& e) {
    std::cerr << "error: " << e.what() << std::endl;
    return (1);
  }
  return (0);
}
