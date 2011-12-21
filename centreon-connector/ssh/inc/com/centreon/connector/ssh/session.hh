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

#ifndef CCCS_SESSION_HH
#  define CCCS_SESSION_HH

#  include <libssh2.h>
#  include <list>
#  include <queue>
#  include <string>
#  include <sys/types.h>
#  include "com/centreon/connector/ssh/namespace.hh"
#  include "com/centreon/connector/ssh/socket_handle.hh"
#  include "com/centreon/handle_listener.hh"

CCCS_BEGIN()

// Forward declaration.
class                   channel;

/**
 *  @class session session.hh "com/centreon/connector/ssh/session.hh"
 *  @brief SSH session.
 *
 *  SSH session between Centreon Connector SSH and a remote
 *  host. The session is kept open as long as needed.
 */
class                   session : public com::centreon::handle_listener {
public:
                        session(
                          std::string const& host,
                          std::string const& user,
                          std::string const& password);
                        ~session() throw ();
  void                  close();
  void                  close(handle& h);
  bool                  empty() const;
  void                  error(handle& h);
  void                  open();
  void                  read(handle& h);
  void                  run(
                          std::string const& cmd,
                          unsigned long long cmd_id,
                          time_t timeout);
  bool                  want_read(handle& h);
  bool                  want_write(handle& h);
  void                  write(handle& h);

private:
  enum                  e_step {
    session_connect = 0,
    session_startup,
    session_password,
    session_key,
    session_exec
  };
  struct                s_command {
    std::string         cmd;
    unsigned long long  cmd_id;
    time_t              timeout;
  };

                        session(session const& s);
  session&              operator=(session const& s);
  void                  _connect();
  void                  _exec();
  void                  _key();
  void                  _passwd();
  void                  _run();
  void                  _startup();

  std::list<channel*>   _channels;
  std::queue<s_command> _commands;
  std::string           _host;
  std::string           _password;
  LIBSSH2_SESSION*      _session;
  socket_handle         _socket;
  e_step                _step;
  std::string           _user;
};

CCCS_END()

#endif // !CCCS_SESSION_HH
