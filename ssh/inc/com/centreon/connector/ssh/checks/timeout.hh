/*
** Copyright 2011-2012 Merethis
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

#ifndef CCCS_CHECKS_TIMEOUT_HH
#  define CCCS_CHECKS_TIMEOUT_HH

#  include <stddef.h>
#  include "com/centreon/connector/ssh/namespace.hh"
#  include "com/centreon/task.hh"

CCCS_BEGIN()

namespace    checks {
  // Forward declaration.
  class      check;

  /**
   *  @class timeout timeout.hh "com/centreon/connector/ssh/checks/timeout.hh"
   *  @brief Check timeout.
   *
   *  Task executed when a check timeouts.
   */
  class      timeout : public com::centreon::task {
  public:
             timeout(check* chk = NULL);
             timeout(timeout const& t);
             ~timeout() throw ();
    timeout& operator=(timeout const& t);
    check*   get_check() const throw ();
    void     run();
    void     set_check(check* chk) throw ();

  private:
    void     _internal_copy(timeout const& t);

    check*   _check;
  };
}

CCCS_END()

#endif // !CCCS_CHECKS_TIMEOUT_HH
