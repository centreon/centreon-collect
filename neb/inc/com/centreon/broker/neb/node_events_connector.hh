/*
** Copyright 2015 Merethis
**
** This file is part of Centreon Broker.
**
** Centreon Broker is free software: you can redistribute it and/or
** modify it under the terms of the GNU General Public License version 2
** as published by the Free Software Foundation.
**
** Centreon Broker is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
** General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with Centreon Broker. If not, see
** <http://www.gnu.org/licenses/>.
*/

#ifndef CCB_NEB_NODE_EVENTS_CONNECTOR_HH
#  define CCB_NEB_NODE_EVENTS_CONNECTOR_HH

#  include "com/centreon/broker/io/endpoint.hh"
#  include "com/centreon/broker/namespace.hh"
#  include "com/centreon/broker/database_config.hh"

CCB_BEGIN()

namespace                        neb {
  /**
   *  @class node_events_connector node_events_connector.hh "com/centreon/broker/neb/node_events_connector.hh"
   *  @brief Open a correlation stream.
   *
   *  Generate a correlation stream that will generation correlation
   *  events (issue, issue parenting, host/service state events, ...).
   */
  class                          node_events_connector : public io::endpoint {
  public:
                                 node_events_connector(
                                   misc::shared_ptr<persistent_cache> cache,
                                   database_config const& conf);
                                 node_events_connector(node_events_connector const& other);
                                 ~node_events_connector();
    node_events_connector&       operator=(node_events_connector const& other);
    io::endpoint*                clone() const;
    void                         close();
    misc::shared_ptr<io::stream> open();
    misc::shared_ptr<io::stream> open(QString const& id);

  private:
    misc::shared_ptr<persistent_cache>
                                 _cache;
    database_config              _conf;
  };
}

CCB_END()

#endif // !CCB_NEB_NODE_EVENTS_CONNECTOR_HH
