/*
** Copyright 2011-2013 Merethis
**
** This file is part of Centreon Perl Connector.
**
** Centreon Perl Connector is free software: you can redistribute it
** and/or modify it under the terms of the GNU Affero General Public
** License as published by the Free Software Foundation, either version
** 3 of the License, or (at your option) any later version.
**
** Centreon Perl Connector is distributed in the hope that it will be
** useful, but WITHOUT ANY WARRANTY; without even the implied warranty
** of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
** Affero General Public License for more details.
**
** You should have received a copy of the GNU Affero General Public
** License along with Centreon Perl Connector. If not, see
** <http://www.gnu.org/licenses/>.
*/

#ifndef CCCP_ORDERS_LISTENER_HH
#  define CCCP_ORDERS_LISTENER_HH

#  include <string>
#  include <time.h>
#  include "com/centreon/connector/perl/namespace.hh"

CCCP_BEGIN()

namespace        orders {
  /**
   *  @class listener listener.hh "com/centreon/connector/perl/orders/listener.hh"
   *  @brief Listen orders issued by the monitoring engine.
   *
   *  Wait for orders from the monitoring engine and take actions
   *  accordingly.
   */
  class          listener {
  public:
                 listener();
                 listener(listener const& l);
    virtual      ~listener();
    listener&    operator=(listener const& l);
    virtual void on_eof() = 0;
    virtual void on_error() = 0;
    virtual void on_execute(
                   unsigned long long cmd_id,
                   time_t timeout,
                   std::string const& cmd) = 0;
    virtual void on_quit() = 0;
    virtual void on_version() = 0;
  };
}

CCCP_END()

#endif // !CCCP_ORDERS_LISTENER_HH
