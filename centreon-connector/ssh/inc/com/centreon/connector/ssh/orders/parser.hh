/*
** Copyright 2011-2013 Centreon
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
**
** For more information : contact@centreon.com
*/

#ifndef CCCS_ORDERS_PARSER_HH
#  define CCCS_ORDERS_PARSER_HH

#  include <string>
#  include "com/centreon/connector/ssh/namespace.hh"
#  include "com/centreon/connector/ssh/orders/listener.hh"
#  include "com/centreon/handle_listener.hh"

CCCS_BEGIN()

namespace              orders {
  /**
   *  @class parser parser.hh "com/centreon/connector/ssh/orders/parser.hh"
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

CCCS_END()

#endif // !CCCS_ORDERS_PARSER_HH
