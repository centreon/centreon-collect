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

#include <libssh2.h>

#include "com/centreon/connector/result.hh"

namespace com::centreon::connector::ssh {

namespace sessions {
class session;
}

namespace checks {

/**
 *  @class check check.hh "com/centreon/connector/ssh/checks/check.hh"
 *  @brief Execute a check on a host.
 *
 *  Execute a check by opening a new channel on a SSH session.
 */
class check : public std::enable_shared_from_this<check> {
  using callback = std::function<void(const result&)>;

 public:
  using pointer = std::shared_ptr<check>;
  using string_list = std::list<std::string>;

  check(const std::shared_ptr<sessions::session>& session,
        unsigned long long cmd_id, string_list const& cmds,
        const time_point& tmt, int skip_stdout = -1, int skip_stderr = -1);
  ~check() noexcept;

  template <class callback_type>
  void execute(callback_type&& callback);

  void dump(std::ostream& s) const;

 private:
  enum class e_step {
    chan_open = 1,
    chan_exec,
    chan_read_stdout,
    chan_read_stderr,
    chan_close
  };

  check(check const& c) = delete;
  check& operator=(check const& c) = delete;

  void _execute();
  void _process();
  void _close();
  void _close_handler(int retval);
  void _exec();
  void _open();
  void _read(bool read_stdout);
  static std::string& _skip_data(std::string& data, int nb_line);

  LIBSSH2_CHANNEL* _channel;
  unsigned long long _cmd_id;
  string_list _cmds;
  time_point _timeout;
  std::shared_ptr<sessions::session> _session;
  callback _callback;
  int _skip_stderr;
  int _skip_stdout;
  std::string _stderr;
  std::string _stdout;
  e_step _step;
};

template <class callback_type>
void check::execute(callback_type&& callback) {
  _callback = callback;
  _execute();
}

std::ostream& operator<<(std::ostream& os, const check& chk);

}  // namespace checks

}  // namespace com::centreon::connector::ssh

namespace fmt {
// formatter specializations for fmt
template <>
struct formatter<com::centreon::connector::ssh::checks::check>
    : ostream_formatter {};
}  // namespace fmt

#endif  // !CCCS_CHECKS_CHECK_HH
