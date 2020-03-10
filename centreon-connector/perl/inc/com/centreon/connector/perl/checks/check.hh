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

#ifndef CCCP_CHECKS_CHECK_HH
#define CCCP_CHECKS_CHECK_HH

#include <sys/types.h>
#include <string>
#include "com/centreon/connector/perl/namespace.hh"
#include "com/centreon/connector/perl/pipe_handle.hh"
#include "com/centreon/handle_listener.hh"
#include "com/centreon/timestamp.hh"

CCCP_BEGIN()

namespace checks {
// Forward declarations.
class listener;
class result;

/**
 *  @class check check.hh "com/centreon/connector/perl/checks/check.hh"
 *  @brief Perl check.
 *
 *  Class wrapping a Perl check as requested by the monitoring engine.
 */
class check : public handle_listener {
 public:
  check();
  ~check() throw();
  void error(handle& h);
  pid_t execute(unsigned long long cmd_id,
                std::string const& cmd,
                const timestamp& tmt);
  void listen(listener* listnr);
  void on_timeout(bool final = true);
  void read(handle& h);
  void terminated(int exit_code);
  void unlisten(listener* listnr);
  bool want_read(handle& h);
  void write(handle& h);

 private:
  check(check const& c);
  check& operator=(check const& c);
  void _send_result_and_unregister(result const& r);

  pid_t _child;
  unsigned long long _cmd_id;
  pipe_handle _err;
  listener* _listnr;
  pipe_handle _out;
  std::string _stderr;
  std::string _stdout;
  unsigned long _timeout;
};
}  // namespace checks

CCCP_END()

#endif  // !CCCP_CHECKS_CHECK_HH
