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

#ifndef CCCS_SESSIONS_SESSION_HH
#define CCCS_SESSIONS_SESSION_HH

#include <libssh2.h>
#include <set>
#include "com/centreon/connector/ssh/namespace.hh"
#include "com/centreon/connector/ssh/sessions/credentials.hh"
#include "com/centreon/connector/ssh/sessions/listener.hh"
#include "com/centreon/connector/ssh/sessions/socket_handle.hh"
#include "com/centreon/handle_listener.hh"

CCCS_BEGIN()

namespace sessions {
/**
 *  @class session session.hh "com/centreon/connector/ssh/session.hh"
 *  @brief SSH session.
 *
 *  SSH session between Centreon SSH Connector and a remote
 *  host. The session is kept open as long as needed.
 */
class session : public com::centreon::handle_listener {
 public:
  session(credentials const& creds);
  ~session() noexcept;
  session(session const& s) = delete;
  session& operator=(session const& s) = delete;
  void close();
  void connect(bool use_ipv6 = false);
  void error();
  void error(handle& h);
  credentials const& get_credentials() const noexcept;
  LIBSSH2_SESSION* get_libssh2_session() const noexcept;
  socket_handle* get_socket_handle() noexcept;
  bool is_connected() const noexcept;
  void listen(listener* listnr);
  LIBSSH2_CHANNEL* new_channel();
  void read(handle& h);
  void unlisten(listener* listnr);
  bool want_read(handle& h);
  bool want_write(handle& h);
  void write(handle& h);

 private:
  enum e_step {
    session_startup = 0,
    session_password,
    session_key,
    session_keepalive,
    session_error
  };

  void _available();
  void _key();
  void _passwd();
  void _startup();

  credentials _creds;
  std::set<listener*> _listnrs;
  std::set<listener*>::iterator _listnrs_it;
  bool _needed_new_chan;
  LIBSSH2_SESSION* _session;
  socket_handle _socket;
  e_step _step;
  char const* _step_string;
};
}  // namespace sessions

CCCS_END()

#endif  // !CCCS_SESSIONS_SESSION_HH
