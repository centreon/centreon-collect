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

#ifndef CCCS_ORDERS_OPTIONS_HH
#  define CCCS_ORDERS_OPTIONS_HH

#  include <list>
#  include <string>
#  include "com/centreon/connector/ssh/namespace.hh"

CCCS_BEGIN()

namespace                         orders {
  /**
   *  @class options options.hh "com/centreon/connector/ssh/orders/options.hh"
   *  @brief Parse orders command line arguments.
   *
   *  Parse orders command line arguments.
   */
  class                           options {
  public:
    enum                          ip_protocol {
      ip_v4 = 0,
      ip_v6 = 1
    };

                                  options(std::string const& cmdline = "");
                                  options(options const& p);
                                  ~options() throw ();
    options&                      operator=(options const& p);
    std::string const&            get_authentication() const throw ();
    std::list<std::string> const& get_commands() const throw ();
    std::string const&            get_host() const throw ();
    std::string const&            get_identity_file() const throw ();
    ip_protocol                   get_ip_protocol() const throw ();
    unsigned short                get_port() const throw ();
    unsigned int                  get_timeout() const throw ();
    std::string const&            get_user() const throw ();
    static std::string            help();
    void                          parse(std::string const& cmdline);
    bool                          skip_stderr() const throw ();
    bool                          skip_stdout() const throw ();

  private:
    void                          _copy(options const& p);

    std::string                   _authentication;
    std::list<std::string>        _commands;
    std::string                   _host;
    std::string                   _identity_file;
    ip_protocol                   _ip_protocol;
    unsigned short                _port;
    bool                          _skip_stderr;
    bool                          _skip_stdout;
    unsigned int                  _timeout;
    std::string                   _user;
  };
}

CCCS_END()

#endif // !CCCS_ORDERS_OPTIONS_HH

