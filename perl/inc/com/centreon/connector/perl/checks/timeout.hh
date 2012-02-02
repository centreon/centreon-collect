/*
** Copyright 2011-2012 Merethis
**
** This file is part of Centreon Connector Perl.
**
** Centreon Connector Perl is free software: you can redistribute it
** and/or modify it under the terms of the GNU Affero General Public
** License as published by the Free Software Foundation, either version
** 3 of the License, or (at your option) any later version.
**
** Centreon Connector Perl is distributed in the hope that it will be
** useful, but WITHOUT ANY WARRANTY; without even the implied warranty
** of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
** Affero General Public License for more details.
**
** You should have received a copy of the GNU Affero General Public
** License along with Centreon Connector Perl. If not, see
** <http://www.gnu.org/licenses/>.
*/

#ifndef CCCP_CHECKS_TIMEOUT_HH
#  define CCCP_CHECKS_TIMEOUT_HH

#  include <stddef.h>
#  include "com/centreon/connector/perl/namespace.hh"
#  include "com/centreon/task.hh"

CCCP_BEGIN()

namespace    checks {
  // Forward declaration.
  class      check;

  /**
   *  @class timeout timeout.hh "com/centreon/connector/perl/checks/timeout.hh"
   *  @brief Check timeout.
   *
   *  Task executed when a check timeouts.
   */
  class      timeout : public com::centreon::task {
  public:
             timeout(check* chk = NULL, bool final = false);
             timeout(timeout const& t);
             ~timeout() throw ();
    timeout& operator=(timeout const& t);
    check*   get_check() const throw ();
    bool     is_final() const throw ();
    void     run();
    void     set_check(check* chk) throw ();
    void     set_final(bool final) throw ();

  private:
    void     _internal_copy(timeout const& t);

    check*   _check;
    bool     _final;
  };
}

CCCP_END()

#endif // !CCCP_CHECKS_TIMEOUT_HH
