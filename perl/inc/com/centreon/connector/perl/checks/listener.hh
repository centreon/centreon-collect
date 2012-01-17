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

#ifndef CCCP_CHECKS_LISTENER_HH
#  define CCCP_CHECKS_LISTENER_HH

#  include "com/centreon/connector/perl/checks/result.hh"
#  include "com/centreon/connector/perl/namespace.hh"

CCCP_BEGIN()

namespace        checks {
  /**
   *  @class listener listener.hh "com/centreon/connector/perl/checks/listener.hh"
   *  @brief Check listener.
   *
   *  Listen check events.
   */
  class          listener {
  public:
                 listener();
                 listener(listener const& l);
    virtual      ~listener();
    listener&    operator=(listener const& l);
    virtual void on_result(result const& result) = 0;
  };
}

CCCP_END()

#endif // !CCCP_CHECKS_LISTENER_HH
