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

#ifndef CCCP_ORDERS_PARSER_HH
#  define CCCP_ORDERS_PARSER_HH

#  include <string>
#  include "com/centreon/connector/perl/namespace.hh"
#  include "com/centreon/connector/perl/orders/listener.hh"
#  include "com/centreon/handle_listener.hh"

CCCP_BEGIN()

namespace              orders {
  /**
   *  @class parser parser.hh "com/centreon/connector/perl/orders/parser.hh"
   *  @brief Parse orders.
   *
   *  Parse orders, generally issued by the monitoring engine. The
   *  parser class can handle be registered with one handle at a time
   *  and one listener.
   */
  class                parser : public handle_listener {
  public:
                       parser();
                       parser(parser const& p);
                       ~parser() throw ();
    parser&            operator=(parser const& p);
    void               error(handle& h);
    std::string const& get_buffer() const throw ();
    listener*          get_listener() const throw ();
    void               listen(listener* l = NULL) throw ();
    void               read(handle& h);
    bool               want_read(handle& h);
    bool               want_write(handle& h);

  private:
    void               _copy(parser const& p);
    void               _parse(std::string const& cmd);

    std::string        _buffer;
    listener*          _listnr;
  };
}

CCCP_END()

#endif // !CCCP_ORDERS_PARSER_HH
