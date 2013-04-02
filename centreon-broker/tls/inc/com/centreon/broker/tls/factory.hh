/*
** Copyright 2013 Merethis
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

#ifndef CCB_TLS_FACTORY_HH
#  define CCB_TLS_FACTORY_HH

#  include "com/centreon/broker/io/factory.hh"
#  include "com/centreon/broker/namespace.hh"

CCB_BEGIN()

namespace          tls {
  /**
   *  @class factory factory.hh "com/centreon/broker/tls/factory.hh"
   *  @brief TLS security layer factory.
   *
   *  Build TLS security objects.
   */
  class            factory : public io::factory {
  public:
                   factory();
                   factory(factory const& right);
                   ~factory();
    factory&       operator=(factory const& right);
    io::factory*   clone() const;
    bool           has_endpoint(
                     config::endpoint& cfg,
                     bool is_input,
                     bool is_output) const;
    bool           has_not_endpoint(
                     config::endpoint& cfg,
                     bool is_input,
                     bool is_output) const;
    io::endpoint*  new_endpoint(
                     config::endpoint& cfg,
                     bool is_input,
                     bool is_output,
                     io::endpoint const* temporary,
                     bool& is_acceptor) const;
    misc::shared_ptr<io::stream>
                   new_stream(
                     misc::shared_ptr<io::stream> to,
                     bool is_acceptor,
                     QString const& proto_name);
  };
}

CCB_END()

#endif // !CCB_TLS_FACTORY_HH
