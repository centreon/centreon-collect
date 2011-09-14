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

#ifndef CCC_SSH_SESSION_HH_
# define CCC_SSH_SESSION_HH_

# include <libssh2.h>
# include <list>
# include <queue>
# include <string>
# include <sys/types.h>
# include "com/centreon/connector/ssh/namespace.hh"

CCC_SSH_BEGIN()

// Forward declaration.
class                   channel;

/**
 *  @class session session.hh "com/centreon/connector/ssh/session.hh"
 *  @brief SSH session.
 *
 *  SSH session between Centreon Connector SSH and a remote
 *  host. The session is kept open as long as needed.
 */
class                   session {
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
  std::list<channel*>   _channels;
  std::queue<s_command> _commands;
  std::string           _host;
  std::string           _password;
  LIBSSH2_SESSION*      _session;
  int                   _socket;
  e_step                _step;
  std::string           _user;
  void                  _connect();
  void                  _exec();
  void                  _key();
  void                  _passwd();
  void                  _run();
  void                  _startup();
                        session(session const& s);
  session&              operator=(session const& s);

 public:
                        session(std::string const& host,
                          std::string const& user,
                          std::string const& password);
                        ~session();
  bool                  empty() const;
  time_t                get_timeout();
  void                  read();
  bool                  read_wanted() const;
  void                  run(std::string const& cmd,
                          unsigned long long cmd_id,
                          time_t timeout);
  int                   socket() const;
  void                  timeout(time_t now);
  void                  write();
  bool                  write_wanted() const;
};

CCC_SSH_END()

#endif /* !CCC_SSH_SESSION_HH_ */
