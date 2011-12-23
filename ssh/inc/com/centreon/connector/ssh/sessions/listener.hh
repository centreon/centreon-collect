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

#ifndef CCCS_SESSIONS_LISTENER_HH
#  define CCCS_SESSIONS_LISTENER_HH

#  include "com/centreon/connector/ssh/namespace.hh"

CCCS_BEGIN()

namespace        sessions {
  // Forward declaration.
  class          session;

  /**
   *  @class listener listener.hh "com/centreon/connector/ssh/sessions/listener.hh"
   *  @brief Session listener.
   *
   *  Listen session events.
   */
  class          listener {
  public:
                 listener();
                 listener(listener const& l);
    virtual      ~listener();
    listener&    operator=(listener const& l);
    virtual void on_close(session& s) = 0;
    virtual void on_connected(session& s) = 0;
  };
}

CCCS_END()

#endif // !CCCS_SESSIONS_LISTENER_HH
