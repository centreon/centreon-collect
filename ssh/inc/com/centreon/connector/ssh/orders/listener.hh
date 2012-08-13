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

#ifndef CCCS_ORDERS_LISTENER_HH
#  define CCCS_ORDERS_LISTENER_HH

#  include <list>
#  include <string>
#  include <time.h>
#  include "com/centreon/connector/ssh/namespace.hh"

CCCS_BEGIN()

namespace        orders {
  /**
   *  @class listener listener.hh "com/centreon/connector/ssh/orders/listener.hh"
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
    virtual void on_error(
                   unsigned long long cmd_id,
                   char const* msg) = 0;
    virtual void on_execute(
                   unsigned long long cmd_id,
                   time_t timeout,
                   std::string const& host,
                   unsigned short port,
                   std::string const& user,
                   std::string const& password,
                   std::string const& identity,
                   std::list<std::string> const& cmds,
                   int skip_stdout,
                   int skip_stderr,
                   bool is_ipv6) = 0;
    virtual void on_quit() = 0;
    virtual void on_version() = 0;
  };
}

CCCS_END()

#endif // !CCCS_ORDERS_LISTENER_HH
