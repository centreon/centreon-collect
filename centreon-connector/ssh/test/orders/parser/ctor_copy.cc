/*
** Copyright 2011 Merethis
**
** This file is part of Centreon Connector SSH.
**
** Centreon Connector SSH is free software: you can redistribute it
** and/or modify it under the terms of the GNU Affero General Public
** License as published by the Free Software Foundation, either version
** 3 of the License, or (at your option) any later version.
**
** Centreon Connector SSH is distributed in the hope that it will be
** useful, but WITHOUT ANY WARRANTY; without even the implied warranty
** of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
** Affero General Public License for more details.
**
** You should have received a copy of the GNU Affero General Public
** License along with Centreon Connector SSH. If not, see
** <http://www.gnu.org/licenses/>.
*/

#include "com/centreon/connector/ssh/orders/parser.hh"
#include "com/centreon/logging/engine.hh"
#include "test/orders/buffer_handle.hh"
#include "test/orders/fake_listener.hh"

using namespace com::centreon::connector::ssh::orders;

#define DATA "765\0merethis\0Centreon is beautiful"

/**
 *  Check that the orders parser can be copy-constructed.
 *
 *  @return 0 on success.
 */
int main() {
  // Initialization.
  com::centreon::logging::engine::load();

  // Data.
  buffer_handle bh;
  bh.write(DATA, sizeof(DATA));

  // Listener.
  fake_listener fl;

  // Base object.
  parser p1;
  p1.listen(&fl);
  p1.read(bh);

  // Copy.
  parser p2(p1);

  // Check.
  return ((p1.get_buffer() != std::string(DATA, sizeof(DATA)))
          || (p1.get_listener() != &fl)
          || (p2.get_buffer() != std::string(DATA, sizeof(DATA)))
          || (p2.get_listener() != &fl));
}
