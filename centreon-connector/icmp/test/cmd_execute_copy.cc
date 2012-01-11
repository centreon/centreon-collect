/*
** Copyright 2011 Merethis
**
** This file is part of Centreon Connector ICMP.
**
** Centreon Connector ICMP is free software: you can redistribute it
** and/or modify it under the terms of the GNU Affero General Public
** License as published by the Free Software Foundation, either version
** 3 of the License, or (at your option) any later version.
**
** Centreon Connector ICMP is distributed in the hope that it will be
** useful, but WITHOUT ANY WARRANTY; without even the implied warranty
** of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
** Affero General Public License for more details.
**
** You should have received a copy of the GNU Affero General Public
** License along with Centreon Connector ICMP. If not, see
** <http://www.gnu.org/licenses/>.
*/

#include <iostream>
#include "com/centreon/exceptions/basic.hh"
#include "com/centreon/connector/icmp/cmd_execute.hh"
#include "com/centreon/logging/engine.hh"

using namespace com::centreon::connector::icmp;
using namespace com::centreon;

/**
 *  Check command execution constructor.
 *
 *  @return 0 on success.
 */
int main() {
  int ret(0);
  logging::engine::load();
  try {
    cmd_execute ref(1);
    ref.execute("127.0.0.1");

    cmd_execute ce1(ref);
    if (ref.get_status() != ce1.get_status()
        || ref.get_message() != ce1.get_message())
      throw (basic_error() << "cmd_execute copy constructor failed");

    cmd_execute ce2 = ref;
    if (ref.get_status() != ce2.get_status()
        || ref.get_message() != ce2.get_message())
      throw (basic_error() << "cmd_execute copy operator failed");
  }
  catch (std::exception const& e) {
    std::cerr << "error: " << e.what() << std::endl;
    ret = 1;
  }
  logging::engine::unload();
  return (ret);
}
