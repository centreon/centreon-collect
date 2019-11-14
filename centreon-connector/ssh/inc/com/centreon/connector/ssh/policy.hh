/*
** Copyright 2011-2019 Centreon
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

#ifndef CCCS_POLICY_HH
#define CCCS_POLICY_HH

#include <map>
#include <mutex>
#include <utility>
#include "com/centreon/connector/ssh/checks/listener.hh"
#include "com/centreon/connector/ssh/orders/listener.hh"
#include "com/centreon/connector/ssh/orders/parser.hh"
#include "com/centreon/connector/ssh/reporter.hh"
#include "com/centreon/connector/ssh/sessions/credentials.hh"
#include "com/centreon/io/file_stream.hh"

CCCS_BEGIN()

// Forward declarations.
namespace checks {
class check;
class result;
}  // namespace checks
namespace sessions {
class session;
}

/**
 *  @class policy policy.hh "com/centreon/connector/ssh/policy.hh"
 *  @brief Software policy.
 *
 *  Manage program execution.
 */
class policy : public orders::listener, public checks::listener {
 public:
  policy();
  ~policy() throw();
  void on_eof();
  void on_error(uint64_t cmd_id, char const* msg);
  void on_execute(uint64_t cmd_id,
                  time_t timeout,
                  std::string const& host,
                  unsigned short port,
                  std::string const& user,
                  std::string const& password,
                  std::string const& key,
                  std::list<std::string> const& cmds,
                  int skip_output,
                  int skip_error,
                  bool is_ipv6);
  void on_quit();
  void on_result(checks::result const& r);
  void on_version();
  bool run();

 private:
  policy(policy const& p);
  policy& operator=(policy const& p);

  std::map<uint64_t, std::pair<checks::check*, sessions::session*> >
      _checks;
  bool _error;
  std::mutex _mutex;
  orders::parser _parser;
  reporter _reporter;
  std::map<sessions::credentials, sessions::session*> _sessions;
  io::file_stream _sin;
  io::file_stream _sout;
};

CCCS_END()

#endif  // !CCCS_POLICY_HH
