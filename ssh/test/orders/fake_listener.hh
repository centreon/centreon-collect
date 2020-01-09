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

#ifndef TEST_ORDERS_FAKE_LISTENER_HH
#define TEST_ORDERS_FAKE_LISTENER_HH

#include <list>
#include "com/centreon/connector/ssh/orders/listener.hh"

/**
 *  @class fake_listener fake_listener.hh "test/orders/fake_listener.hh"
 *  @brief Fake orders listener.
 *
 *  Register callback call order.
 */
class fake_listener : public com::centreon::connector::ssh::orders::listener {
 public:
  enum e_callback {
    cb_eof,
    cb_error,
    cb_execute,
    cb_quit,
    cb_version
  };
  struct callback_info {
    e_callback callback;
    uint64_t cmd_id;
    time_t timeout;
    std::string host;
    unsigned short port;
    std::string user;
    std::string password;
    std::string identity;
    std::list<std::string> cmds;
    int skip_stderr;
    int skip_stdout;
    bool is_ipv6;
  };

  fake_listener() = default;
  ~fake_listener() = default;
  fake_listener(fake_listener const& fl) = delete;
  fake_listener& operator=(fake_listener const& fl) = delete;
  std::list<callback_info> const& get_callbacks() const throw();
  void on_eof();
  void on_error(uint64_t cmd_id, char const* msg);
  void on_execute(uint64_t cmd_id,
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
  void on_quit();
  void on_version();

 private:
  void _copy(fake_listener const& fl);

  std::list<callback_info> _callbacks;
};

bool operator==(std::list<fake_listener::callback_info> const& left,
                std::list<fake_listener::callback_info> const& right);
bool operator!=(std::list<fake_listener::callback_info> const& left,
                std::list<fake_listener::callback_info> const& right);

#endif  // !TEST_ORDERS_FAKE_LISTENER_HH
