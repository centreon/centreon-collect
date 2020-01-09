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

#ifndef CCCS_CHECKS_CHECK_HH
#define CCCS_CHECKS_CHECK_HH

#include <ctime>
#include <list>
#include <string>
#include "com/centreon/connector/ssh/checks/listener.hh"
#include "com/centreon/connector/ssh/namespace.hh"
#include "com/centreon/connector/ssh/sessions/listener.hh"
#include "com/centreon/connector/ssh/sessions/session.hh"

CCCS_BEGIN()

namespace checks {
// Forward declaration.
class result;

/**
 *  @class check check.hh "com/centreon/connector/ssh/checks/check.hh"
 *  @brief Execute a check on a host.
 *
 *  Execute a check by opening a new channel on a SSH session.
 */
class check : public sessions::listener {
 public:
  check(int skip_stdout = -1, int skip_stderr = -1);
  ~check() throw();
  void execute(sessions::session& sess,
               unsigned long long cmd_id,
               std::list<std::string> const& cmds,
               time_t tmt);
  void listen(checks::listener* listnr);
  void on_available(sessions::session& sess);
  void on_close(sessions::session& sess);
  void on_connected(sessions::session& sess);
  void on_timeout();
  void unlisten(checks::listener* listnr);

 private:
  enum e_step { chan_open = 1, chan_exec, chan_read, chan_close };

  check(check const& c);
  check& operator=(check const& c);
  bool _close();
  bool _exec();
  bool _open();
  bool _read();
  void _send_result_and_unregister(result const& r);
  static std::string& _skip_data(std::string& data, int nb_line);

  LIBSSH2_CHANNEL* _channel;
  std::list<std::string> _cmds;
  unsigned long long _cmd_id;
  checks::listener* _listnr;
  sessions::session* _session;
  int _skip_stderr;
  int _skip_stdout;
  std::string _stderr;
  std::string _stdout;
  e_step _step;
  unsigned long _timeout;
};
}  // namespace checks

CCCS_END()

#endif  // !CCCS_CHECKS_CHECK_HH
