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

#ifndef CCCP_POLICY_HH
#define CCCP_POLICY_HH

#include <sys/types.h>
#include <map>
#include "com/centreon/connector/perl/checks/listener.hh"
#include "com/centreon/connector/perl/namespace.hh"
#include "com/centreon/connector/perl/orders/listener.hh"
#include "com/centreon/connector/perl/orders/parser.hh"
#include "com/centreon/connector/perl/reporter.hh"
#include "com/centreon/io/file_stream.hh"
#include "com/centreon/timestamp.hh"

CCCP_BEGIN()

// Forward declarations.
namespace checks {
class check;
class result;
}  // namespace checks

/**
 *  @class policy policy.hh "com/centreon/connector/perl/policy.hh"
 *  @brief Software policy.
 *
 *  Wraps software policy within a class.
 */
class policy : public orders::listener, public checks::listener {
  std::map<pid_t, checks::check*> _checks;
  bool _error;
  orders::parser _parser;
  reporter _reporter;
  io::file_stream _sin;
  io::file_stream _sout;

 public:
  policy();
  ~policy() noexcept;
  policy(policy const& p) = delete;
  policy& operator=(policy const& p) = delete;

  void on_eof();
  void on_error();
  void on_execute(unsigned long long cmd_id,
                  const timestamp& timeout,
                  std::string const& cmd);
  void on_quit();
  void on_result(checks::result const& r);
  void on_version();
  bool run();

};

CCCP_END()

#endif  // !CCCP_POLICY_HH
