/*
** Copyright 2011-2013 Merethis
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
    unsigned long waiting(5);
    timestamp start(timestamp::now());
    concurrency::thread::sleep(waiting);
    timestamp end(timestamp::now());
    timestamp diff(end - start);
    waiting *= 1000;
    if (diff.to_mseconds() > waiting * 1.20)
      throw (basic_error()
             << "waiting more than necessary: "
             << diff.to_mseconds() << "/" << waiting);
    if (diff.to_mseconds() < waiting * 0.90)
      throw (basic_error()
             << "waiting less than necessary: "
             << diff.to_mseconds() << "/" << waiting);
  }
  catch (std::exception const& e) {
    std::cerr << "error: " << e.what() << std::endl;
    return (1);
  }
  return (0);
}
