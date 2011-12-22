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

#ifndef CCCS_CHECKS_LISTENER_HH
#  define CCCS_CHECKS_LISTENER_HH

#  include "com/centreon/connector/ssh/checks/result.hh"
#  include "com/centreon/connector/ssh/namespace.hh"

CCCS_BEGIN()

namespace        checks {
  /**
   *  @class listener listener.hh "com/centreon/connector/ssh/checks/listener.hh"
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

CCCS_END()

#endif // !CCCS_CHECKS_LISTENER_HH
