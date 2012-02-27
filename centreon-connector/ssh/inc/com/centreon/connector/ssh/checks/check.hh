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

#ifndef CCCS_CHECKS_CHECK_HH
#  define CCCS_CHECKS_CHECK_HH

#  include <list>
#  include <string>
#  include <time.h>
#  include "com/centreon/connector/ssh/checks/listener.hh"
#  include "com/centreon/connector/ssh/namespace.hh"
#  include "com/centreon/connector/ssh/sessions/listener.hh"
#  include "com/centreon/connector/ssh/sessions/session.hh"

CCCS_BEGIN()

namespace                  checks {
  // Forward declaration.
  class                    result;

  /**
   *  @class check check.hh "com/centreon/connector/ssh/checks/check.hh"
   *  @brief Execute a check on a host.
   *
   *  Execute a check by opening a new channel on a SSH session.
   */
  class                    check : public sessions::listener {
  public:
                           check(
                             int skip_stdout = -1,
                             int skip_stderr = -1);
                           ~check() throw ();
    void                   execute(
                             sessions::session& sess,
                             unsigned long long cmd_id,
                             std::list<std::string> const& cmds,
                             time_t tmt);
    void                   listen(checks::listener* listnr);
    void                   on_available(sessions::session& sess);
    void                   on_close(sessions::session& sess);
    void                   on_connected(sessions::session& sess);
    void                   on_timeout();
    void                   unlisten(checks::listener* listnr);

  private:
    enum                   e_step {
      chan_open = 1,
      chan_exec,
      chan_read,
      chan_close
    };

                           check(check const& c);
    check&                 operator=(check const& c);
    bool                   _close();
    bool                   _exec();
    bool                   _open();
    bool                   _read();
    void                   _send_result_and_unregister(result const& r);
    static std::string&    _skip_data(std::string& data, int nb_line);

    LIBSSH2_CHANNEL*       _channel;
    std::list<std::string> _cmds;
    unsigned long long     _cmd_id;
    checks::listener*      _listnr;
    sessions::session*     _session;
    int                    _skip_stderr;
    int                    _skip_stdout;
    std::string            _stderr;
    std::string            _stdout;
    e_step                 _step;
    unsigned long          _timeout;
  };
}

CCCS_END()

#endif // !CCCS_CHECKS_CHECK_HH
