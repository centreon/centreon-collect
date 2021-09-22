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

#ifndef CCCS_ORDERS_LISTENER_HH
#define CCCS_ORDERS_LISTENER_HH

#include <ctime>
#include <list>
#include <string>
#include "com/centreon/connector/ssh/namespace.hh"
#include "com/centreon/timestamp.hh"

CCCS_BEGIN()

namespace orders {
/**
 *  @class listener listener.hh "com/centreon/connector/ssh/orders/listener.hh"
 *  @brief Listen orders issued by the monitoring engine.
 *
 *  Wait for orders from the monitoring engine and take actions
 *  accordingly.
 */
class listener {
 public:
  listener() = default;
  listener(listener const& l) = delete;
  virtual ~listener() = default;
  listener& operator=(listener const& l) = delete;
  virtual void on_eof() = 0;
  virtual void on_error(uint64_t cmd_id, char const* msg) = 0;
  virtual void on_execute(uint64_t cmd_id,
                          const timestamp& timeout,
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
}  // namespace orders

CCCS_END()

#endif  // !CCCS_ORDERS_LISTENER_HH
