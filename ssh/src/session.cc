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

#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <iostream>
#include <libssh2.h>
#include <memory>
#include <netdb.h>
#include <netinet/in.h>
#include <pwd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include "com/centreon/connector/ssh/channel.hh"
#include "com/centreon/connector/ssh/exception.hh"
#include "com/centreon/connector/ssh/session.hh"
#include "com/centreon/connector/ssh/std_io.hh"

using namespace com::centreon::connector::ssh;

/**************************************
*                                     *
*           Private Methods           *
*                                     *
**************************************/

/**
 *  @brief Copy constructor.
 *
 *  Any call to this constructor will result in a call to abort().
 *
 *  @param[in] s Object to copy.
 */
session::session(session const& s) {
  (void)s;
  assert(false);
  abort();
}

/**
 *  @brief Assignment operator.
 *
 *  Any call to this method will result in a call to abort().
 *
 *  @param[in] s Object to copy.
 *
 *  @return This object.
 */
session& session::operator=(session const& s) {
  (void)s;
  assert(false);
  abort();
  return (*this);
}

/**
 *  @brief Perform connection operations.
 *
 *  Once this method is called, this means that socket was successfully
 *  connected to remote host.
 */
void session::_connect() {
  // Create session instance.
  _session = libssh2_session_init();
  if (!_session)
    throw (exception()
             << "SSH session creation failed (out of memory ?)");

  // Launch session startup.
  _step = session_startup;
  _startup();

  return ;
}

/**
 *  Run commands.
 */
void session::_exec() {
  // Run all channels.
  for (std::list<channel*>::iterator
         it = _channels.begin(),
         end = _channels.end();
       it != end;
       ++it) {
    // Delete object is 1) requested by channel or
    // 2) because channel thrown an exception.
    bool to_del(false);
    try {
      to_del = !(*it)->run();
    }
    catch (std::exception const& e) {
      to_del = true;
      std::cerr << e.what() << std::endl;
    }
    catch (...) {
      to_del = true;
    }
    if (to_del) {
      delete *it;
      _channels.erase(it);
      break ;
    }
  }

  // Launch new commands.
  _run();

  return ;
}

/**
 *  Attempt public key authentication.
 */
void session::_key() {
  // Get home directory.
  passwd* pw(getpwuid(getuid()));

  // Build key paths.
  std::string priv;
  std::string pub;
  if (pw && pw->pw_dir) {
    priv = pw->pw_dir;
    priv.append("/");
    pub = pw->pw_dir;
    pub.append("/");
  }
  priv.append(".ssh/id_rsa");
  pub.append(".ssh/id_rsa.pub");

  // Try public key authentication.
  int retval(libssh2_userauth_publickey_fromfile(_session,
               _user.c_str(),
               pub.c_str(),
               priv.c_str(),
               _password.c_str()));
  if (retval < 0) {
    if (retval != LIBSSH2_ERROR_EAGAIN)
      throw (exception() << "user authentication failed");
  }
  else {
    // Enable non-blocking mode.
    libssh2_session_set_blocking(_session, 0);

    // Set execution step.
    _step = session_exec;
  }
}

/**
 *  Try password authentication.
 */
void session::_passwd() {
  // Try password.
  int retval(libssh2_userauth_password(_session,
               _user.c_str(),
               _password.c_str()));
  if (retval != 0) {
    if (retval == LIBSSH2_ERROR_AUTHENTICATION_FAILED) {
      _step = session_key;
      _key();
    }
    else if (retval != LIBSSH2_ERROR_EAGAIN) {
      char* msg;
      libssh2_session_last_error(_session, &msg, NULL, 0);
      throw (exception() << "password authentication failed: "
               << msg << " (error " << retval << ")");
    }
  }
  else {
    // Enable non-blocking mode.
    libssh2_session_set_blocking(_session, 0);

    // Set execution step.
    _step = session_exec;
    _exec();
  }
  return ;
}

/**
 *  Run new commands.
 */
void session::_run() {
  while (!_commands.empty()) {
    s_command& s(_commands.front());
    try {
      std::auto_ptr<channel> ptr(new channel(_session,
        s.cmd,
        s.cmd_id,
        s.timeout));
      _channels.push_back(ptr.get());
      ptr.release();
    }
    catch (...) {
      std_io::instance().submit_check_result(s.cmd_id,
        false,
        -1,
        "",
        "");
    }
    _commands.pop();
  }
  return ;
}

/**
 *  Perform SSH connection startup.
 */
void session::_startup() {
  // Exchange banners, keys, setup crypto, compression, ...
  int retval(libssh2_session_startup(_session, _socket));
  if (retval) {
    if (retval != LIBSSH2_ERROR_EAGAIN) { // Fatal failure.
      char* msg;
      int code(libssh2_session_last_error(_session, &msg, NULL, 0));
      throw (exception() << "failure establishing SSH session: "
               << msg << " (error " << code << ")");
    }
  }
  else { // Successful startup.
    // Initialize known hosts list.
    LIBSSH2_KNOWNHOSTS* known_hosts(libssh2_knownhost_init(_session));
    if (!known_hosts) {
      char* msg;
      libssh2_session_last_error(_session, &msg, NULL, 0);
      throw (exception() << "could not create known hosts list: "
               << msg);
    }

    // Get home directory.
    passwd* pw(getpwuid(getuid()));

    // Read OpenSSH's known hosts file.
    std::string known_hosts_file;
    if (pw && pw->pw_dir) {
      known_hosts_file = pw->pw_dir;
      known_hosts_file.append("/.ssh/");
    }
    known_hosts_file.append("known_hosts");
    libssh2_knownhost_readfile(known_hosts,
      known_hosts_file.c_str(),
      LIBSSH2_KNOWNHOST_FILE_OPENSSH);

    // Check host fingerprint against known hosts.
    {
      // Get peer fingerprint.
      size_t len;
      int type;
      char const* fingerprint(libssh2_session_hostkey(_session,
        &len,
        &type));
      if (!fingerprint) {
        char* msg;
        libssh2_session_last_error(_session, &msg, NULL, 0);
        libssh2_knownhost_free(known_hosts);
        throw (exception() << "failed to get remote host fingerprint: "
                 << msg);
      }

      // Check fingerprint.
      libssh2_knownhost* kh;
#if LIBSSH2_VERSION_NUM >= 0x010206
      // Introduced in 1.2.6.
      int check(libssh2_knownhost_checkp(known_hosts,
        _host.c_str(),
        22,
        fingerprint,
        len,
        LIBSSH2_KNOWNHOST_TYPE_PLAIN | LIBSSH2_KNOWNHOST_KEYENC_RAW,
        &kh));
#else
      // 1.2.5 or older.
      int check(libssh2_knownhost_check(known_hosts,
        _host.c_str(),
        fingerprint,
        len,
        LIBSSH2_KNOWNHOST_TYPE_PLAIN | LIBSSH2_KNOWNHOST_KEYENC_RAW,
        &kh));
#endif // LIBSSH2_VERSION_NUM

      // Free known hosts list.
      libssh2_knownhost_free(known_hosts);

      // Check fingerprint.
      if (check != LIBSSH2_KNOWNHOST_CHECK_MATCH) {
        exception e;
        e << "host '" << _host.c_str()
          << "' is not known or could not be validated: ";
        if (LIBSSH2_KNOWNHOST_CHECK_NOTFOUND == check)
          e << "host was not found in known_hosts file";
        else if (LIBSSH2_KNOWNHOST_CHECK_MISMATCH == check)
          e << "host fingerprint mismatch with known_hosts file";
        else
          e << "unknown error";
        throw (e);
      }

      // Successful peer authentication.
      _step = session_password;
      _passwd();
    }
  }
}

/**************************************
*                                     *
*           Public Methods            *
*                                     *
**************************************/

/**
 *  Constructor.
 *
 *  @param[in] host     Host to connect to.
 *  @param[in] user     User name.
 *  @param[in] password Password.
 */
session::session(std::string const& host,
                 std::string const& user,
                 std::string const& password)
  : _host(host),
    _password(password),
    _session(NULL),
    _socket(-1),
    _step(session_connect),
    _user(user) {
  // Host pointer.
  char const* host_ptr(_host.c_str());

  // Host lookup.
  sockaddr_in sin;
  memset(&sin, 0, sizeof(sin));
  {
    // Try to avoid DNS lookup.
    in_addr_t addr(inet_addr(host_ptr));
    if (addr != (in_addr_t)-1)
      sin.sin_addr.s_addr = addr;
    // DNS lookup.
    else {
      // IPv4 address lookup only.
      addrinfo hint;
      memset(&hint, 0, sizeof(hint));
      hint.ai_family = AF_INET;
      hint.ai_socktype = SOCK_STREAM;
      addrinfo* res;
      int retval(getaddrinfo(host_ptr,
                             NULL,
                             &hint,
                             &res));
      if (retval)
        throw (exception() << "lookup of host '" << host_ptr
                 << "' failed: " << gai_strerror(retval));
      else if (!res)
        throw (exception() << "no IPv4 address found for host '"
                 << host_ptr << "'");

      // Get address.
      sin.sin_addr.s_addr = ((sockaddr_in*)(res->ai_addr))->sin_addr.s_addr;

      // Free result.
      freeaddrinfo(res);
    }
  }

  // Set address info.
  sin.sin_family = AF_INET;
  sin.sin_port = htons(22); // Standard SSH port.

  // Create socket.
  _socket = ::socket(AF_INET, SOCK_STREAM, 0);
  if (_socket < 0) {
    char const* msg(strerror(errno));
    throw (exception() << "socket creation failed: " << msg);
  }

  // Set socket non-blocking.
  int flags(fcntl(_socket, F_GETFL));
  if (flags < 0) {
    char const* msg(strerror(errno));
    ::close(_socket);
    throw (exception() << "could not get socket flags: " << msg);
  }
  flags |= O_NONBLOCK;
  if (fcntl(_socket, F_SETFL, flags) == -1) {
    char const* msg(strerror(errno));
    ::close(_socket);
    throw (exception() << "could not make socket non blocking: "
             << msg);
  }

  // Connect to remote host.
  if ((connect(_socket, (sockaddr*)&sin, sizeof(sin)) != 0)
      && (errno != EINPROGRESS)) {
      char const* msg(strerror(errno));
      ::close(_socket);
      throw (exception() << "could not connect to '" << host_ptr
               << "': " << msg);
  }
}

/**
 *  Destructor.
 */
session::~session() {
  // Shutdown channels.
  for (std::list<channel*>::iterator
         it = _channels.begin(),
         end = _channels.end();
       it != end;
       ++it)
    try {
      delete *it;
    }
    catch (...) {}

  // Send result of not run commands.
  while (!_commands.empty()) {
    std_io::instance().submit_check_result(_commands.front().cmd_id,
      false,
      -1,
      "",
      "");
    _commands.pop();
  }

  // Delete session.
  libssh2_session_set_blocking(_session, 1);
  libssh2_session_disconnect(_session,
    "Centreon Connector SSH shutdown");
  libssh2_session_free(_session);

  // Close socket.
  shutdown(_socket, SHUT_RDWR);
  ::close(_socket);
}

/**
 *  Check if channel list is empty or not.
 *
 *  @return true if the channel list is empty.
 */
bool session::empty() const {
  return (_channels.empty());
}

/**
 *  Get closest command timeout.
 *
 *  @return Closest command timeout.
 */
time_t session::get_timeout() {
  // According to POSIX, maximum timeout interval should be at least
  // 31 days long.
  time_t now(time(NULL));
  time_t timeout(now + 31 * 24 * 60 * 60);
  for (std::list<channel*>::iterator
         it = _channels.begin(),
         end = _channels.end();
       it != end;
       ++it) {
    time_t t((*it)->timeout());
    if (t < timeout)
      timeout = t;
  }
  return (timeout);
}

/**
 *  Read data is available.
 */
void session::read() {
  static void (session::* const redirector[])() = {
      &session::_connect,
      &session::_startup,
      &session::_passwd,
      &session::_key,
      &session::_exec
    };
  (this->*redirector[_step])();
  return ;
}

/**
 *  Check is read monitoring is wanted.
 *
 *  @return true if read monitoring is wanted.
 */
bool session::read_wanted() const {
  return (_session && (libssh2_session_block_directions(_session)
                       & LIBSSH2_SESSION_BLOCK_INBOUND));
}

/**
 *  Run a command.
 *
 *  @param[in] cmd     Command line.
 *  @param[in] cmd_id  Command ID.
 *  @param[in] timeout Command timeout.
 */
void session::run(std::string const& cmd,
                  unsigned long long cmd_id,
                  time_t timeout) {
  s_command s;
  s.cmd = cmd;
  s.cmd_id = cmd_id;
  s.timeout = timeout;
  _commands.push(s);
  return ;
}

/**
 *  Get the socket FD.
 *
 *  @return Socket FD associated with this SSH class.
 */
int session::socket() const {
  return (_socket);
}

/**
 *  Method called when timeout occurs.
 *
 *  @param[in] now Current time.
 */
void session::timeout(time_t now) {
  for (std::list<channel*>::iterator
         it = _channels.begin(),
         end = _channels.end();
       it != end;)
    if ((*it)->timeout() < now) {
      try {
        delete *it;
      }
      catch (...) {}
      _channels.erase(it++);
    }
    else
      ++it;
  return ;
}

/**
 *  Write data is available.
 */
void session::write() {
  read();
  return ;
}

/**
 *  Check if write monitoring is wanted.
 *
 *  @return true if write monitoring is wanted.
 */
bool session::write_wanted() const {
  return (!_session || (libssh2_session_block_directions(_session)
                        & LIBSSH2_SESSION_BLOCK_OUTBOUND));
}
