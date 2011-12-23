/*
** Copyright 2011 Merethis
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

#ifndef CCCS_CHECKS_CHECK_HH
#  define CCCS_CHECKS_CHECK_HH

#  include <string>
#  include <time.h>
#  include "com/centreon/connector/ssh/checks/listener.hh"
#  include "com/centreon/connector/ssh/namespace.hh"
#  include "com/centreon/connector/ssh/sessions/listener.hh"
#  include "com/centreon/connector/ssh/sessions/session.hh"
#  include "com/centreon/handle_listener.hh"

CCCS_BEGIN()

namespace              checks {
  // Forward declaration.
  class                result;

  /**
   *  @class check check.hh "com/centreon/connector/ssh/checks/check.hh"
   *  @brief Execute a check on a host.
   *
   *  Execute a check by opening a new channel on a SSH session.
   */
  class                check : public sessions::listener,
                               public com::centreon::handle_listener {
  public:
                       check();
                       ~check() throw ();
    void               close(handle& h);
    void               error(handle& h);
    void               execute(
                         sessions::session& sess,
                         unsigned long long cmd_id,
                         std::string const& cmd,
                         time_t timeout);
    void               listen(checks::listener* listnr);
    void               on_close(sessions::session& sess);
    void               on_connected(sessions::session& sess);
    void               read(handle& h);
    void               unlisten(checks::listener* listnr);
    bool               want_read(handle& h);
    bool               want_write(handle& h);
    void               write(handle& h);

  private:
    enum               e_step {
      chan_open = 1,
      chan_exec,
      chan_read,
      chan_close
    };

                       check(check const& c);
    check&             operator=(check const& c);
    bool               _close();
    bool               _exec();
    bool               _open();
    bool               _read();
    void               _send_result_and_unregister(result const& r);

    LIBSSH2_CHANNEL*   _channel;
    std::string        _cmd;
    unsigned long long _cmd_id;
    checks::listener*  _listnr;
    sessions::session* _session;
    std::string        _stderr;
    std::string        _stdout;
    e_step             _step;
    time_t             _timeout;
  };
}

CCCS_END()

#endif // !CCCS_CHECKS_CHECK_HH
