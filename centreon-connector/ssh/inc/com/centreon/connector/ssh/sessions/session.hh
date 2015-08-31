/*
** Copyright 2011-2013,2015 Merethis
**
** This file is part of Centreon SSH Connector.
**
** Centreon SSH Connector is free software: you can redistribute it
** and/or modify it under the terms of the GNU Affero General Public
** License as published by the Free Software Foundation, either version
** 3 of the License, or (at your option) any later version.
**
** Centreon SSH Connector is distributed in the hope that it will be
** useful, but WITHOUT ANY WARRANTY; without even the implied warranty
** of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
** Affero General Public License for more details.
**
** You should have received a copy of the GNU Affero General Public
** License along with Centreon SSH Connector. If not, see
** <http://www.gnu.org/licenses/>.
*/

#ifndef CCCS_SESSIONS_SESSION_HH
#  define CCCS_SESSIONS_SESSION_HH

#  include <libssh2.h>
#  include <set>
#  include "com/centreon/connector/ssh/namespace.hh"
#  include "com/centreon/connector/ssh/sessions/credentials.hh"
#  include "com/centreon/connector/ssh/sessions/listener.hh"
#  include "com/centreon/connector/ssh/sessions/socket_handle.hh"
#  include "com/centreon/handle_listener.hh"

CCCS_BEGIN()

namespace                 sessions {
  /**
   *  @class session session.hh "com/centreon/connector/ssh/session.hh"
   *  @brief SSH session.
   *
   *  SSH session between Centreon SSH Connector and a remote
   *  host. The session is kept open as long as needed.
   */
  class                   session : public com::centreon::handle_listener {
  public:
                          session(credentials const& creds);
                          ~session() throw ();
    void                  close();
    void                  connect(bool use_ipv6 = false);
    void                  error();
    void                  error(handle& h);
    credentials const&    get_credentials() const throw ();
    LIBSSH2_SESSION*      get_libssh2_session() const throw ();
    socket_handle*        get_socket_handle() throw ();
    bool                  is_connected() const throw ();
    void                  listen(listener* listnr);
    LIBSSH2_CHANNEL*      new_channel();
    void                  read(handle& h);
    void                  unlisten(listener* listnr);
    bool                  want_read(handle& h);
    bool                  want_write(handle& h);
    void                  write(handle& h);

  private:
    enum                  e_step {
      session_startup = 0,
      session_password,
      session_key,
      session_keepalive,
      session_error
    };

                          session(session const& s);
    session&              operator=(session const& s);
    void                  _available();
    void                  _key();
    void                  _passwd();
    void                  _startup();

    credentials           _creds;
    std::set<listener*>   _listnrs;
    std::set<listener*>::iterator
                          _listnrs_it;
    bool                  _needed_new_chan;
    LIBSSH2_SESSION*      _session;
    socket_handle         _socket;
    e_step                _step;
    char const*           _step_string;
  };
}

CCCS_END()

#endif // !CCCS_SESSIONS_SESSION_HH
