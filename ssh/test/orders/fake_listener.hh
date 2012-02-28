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

#ifndef TEST_ORDERS_FAKE_LISTENER_HH
#  define TEST_ORDERS_FAKE_LISTENER_HH

#  include <list>
#  include "com/centreon/connector/ssh/orders/listener.hh"

/**
 *  @class fake_listener fake_listener.hh "test/orders/fake_listener.hh"
 *  @brief Fake orders listener.
 *
 *  Register callback call order.
 */
class              fake_listener
  : public com::centreon::connector::ssh::orders::listener {
public:
  enum             e_callback {
    cb_eof,
    cb_error,
    cb_execute,
    cb_quit,
    cb_version
  };
  struct           callback_info {
    e_callback     callback;
    unsigned long long
                   cmd_id;
    time_t         timeout;
    std::string    host;
    unsigned short port;
    std::string    user;
    std::string    password;
    std::string    identity;
    std::list<std::string>
                   cmds;
    int            skip_stderr;
    int            skip_stdout;
    bool           is_ipv6;
  };

                   fake_listener();
                   fake_listener(fake_listener const& fl);
                   ~fake_listener();
  fake_listener&   operator=(fake_listener const& fl);
  std::list<callback_info> const&
                   get_callbacks() const throw ();
  void             on_eof();
  void             on_error();
  void             on_execute(
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
                     bool is_ipv6);
  void             on_quit();
  void             on_version();

private:
  void             _copy(fake_listener const& fl);

  std::list<callback_info>
                   _callbacks;
};

bool               operator==(
                     std::list<fake_listener::callback_info> const& left,
                     std::list<fake_listener::callback_info> const& right);
bool               operator!=(
                     std::list<fake_listener::callback_info> const& left,
                     std::list<fake_listener::callback_info> const& right);

#endif // !TEST_ORDERS_FAKE_LISTENER_HH
