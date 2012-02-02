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
#include "com/centreon/connector/ssh/multiplexer.hh"
#include "com/centreon/connector/ssh/sessions/session.hh"
#include "com/centreon/exceptions/basic.hh"
#include "com/centreon/logging/logger.hh"

using namespace com::centreon;
using namespace com::centreon::connector::ssh::sessions;

/**************************************
*                                     *
*           Public Methods            *
*                                     *
**************************************/

/**
 *  Constructor.
 *
 *  @param[in] creds Connection credentials.
 */
session::session(credentials const& creds)
  : _creds(creds),
    _session(NULL),
    _step(session_startup) {
  // Create session instance.
  _session = libssh2_session_init();
  if (!_session)
    throw (basic_error()
             << "SSH session creation failed (out of memory ?)");
}

/**
 *  Destructor.
 */
session::~session() throw () {
  try {
    this->close();
  }
  catch (...) {}

  // Delete session.
  libssh2_session_set_blocking(_session, 1);
  libssh2_session_disconnect(
    _session,
    "Centreon Connector SSH shutdown");
  libssh2_session_free(_session);
}

/**
 *  Close session.
 */
void session::close() {
  // Unregister with multiplexer.
  multiplexer::instance().handle_manager::remove(&_socket);
  multiplexer::instance().handle_manager::remove(this);

  // Notify listeners.
  {
    for (_listnrs_it = _listnrs.begin();
         _listnrs_it != _listnrs.end();
         ++_listnrs_it)
      (*_listnrs_it)->on_close(*this);
  }

  // Close socket.
  _socket.close();

  return ;
}

/**
 *  Open session.
 */
void session::connect() {
  // Check that session wasn't already open.
  if (is_connected()) {
    logging::info(logging::high)
      << "attempt to open already opened session";
    return ;
  }

  // Step.
  _step = session_startup;

  // Host pointer.
  char const* host_ptr(_creds.get_host().c_str());

  // Host lookup.
  logging::info(logging::high) << "looking up address " << host_ptr;
  sockaddr_in sin;
  memset(&sin, 0, sizeof(sin));
  {
    // Try to avoid DNS lookup.
    in_addr_t addr(inet_addr(host_ptr));
    if (addr != (in_addr_t)-1) {
      logging::debug(logging::high) << "host "
        << host_ptr << " is an IP address";
      sin.sin_addr.s_addr = addr;
    }
    // DNS lookup.
    else {
      // IPv4 address lookup only.
      addrinfo hint;
      memset(&hint, 0, sizeof(hint));
      hint.ai_family = AF_INET;
      hint.ai_socktype = SOCK_STREAM;
      addrinfo* res;
      int retval(getaddrinfo(
                   host_ptr,
                   NULL,
                   &hint,
                   &res));
      if (retval)
        throw (basic_error() << "lookup of host '" << host_ptr
                 << "' failed: " << gai_strerror(retval));
      else if (!res)
        throw (basic_error() << "no IPv4 address found for host '"
                 << host_ptr << "'");

      // Log message.
      logging::debug(logging::low) << "found host " << host_ptr
        << " address through name resolution";

      // Get address.
      sin.sin_addr.s_addr
        = ((sockaddr_in*)(res->ai_addr))->sin_addr.s_addr;

      // Free result.
      freeaddrinfo(res);
    }
  }

  // Set address info.
  sin.sin_family = AF_INET;
  sin.sin_port = htons(22); // Standard SSH port.

  // Create socket.
  int mysocket;
  mysocket = ::socket(AF_INET, SOCK_STREAM, 0);
  if (mysocket < 0) {
    char const* msg(strerror(errno));
    throw (basic_error() << "socket creation failed: " << msg);
  }

  // Set socket non-blocking.
  int flags(fcntl(mysocket, F_GETFL));
  if (flags < 0) {
    char const* msg(strerror(errno));
    ::close(mysocket);
    throw (basic_error() << "could not get socket flags: " << msg);
  }
  flags |= O_NONBLOCK;
  if (fcntl(mysocket, F_SETFL, flags) == -1) {
    char const* msg(strerror(errno));
    ::close(mysocket);
    throw (basic_error()
             << "could not make socket non blocking: " << msg);
  }

  // Connect to remote host.
  if ((::connect(mysocket, (sockaddr*)&sin, sizeof(sin)) != 0)
      && (errno != EINPROGRESS)) {
      char const* msg(strerror(errno));
      ::close(mysocket);
      throw (basic_error() << "could not connect to '"
               << host_ptr << "': " << msg);
  }

  // Set socket handle.
  _socket.set_native_handle(mysocket);

  // Register with multiplexer.
  multiplexer::instance().handle_manager::add(&_socket, this, true);

  // Launch the connection process.
  logging::debug(logging::medium)
    << "manually launching the connection process of session "
    << _creds.get_user() << "@" << _creds.get_host();
  _startup();

  return ;
}

/**
 *  Error callback.
 *
 *  @param[in,out] h Handle.
 */
void session::error(handle& h) {
  (void)h;
  logging::error(logging::low)
    << "error detected on socket, shutting down session "
    << _creds.get_user() << "@" << _creds.get_host();
  this->close();
  return ;
}

/**
 *  Get the session credentials.
 *
 *  @return Credentials associated to this session.
 */
credentials const& session::get_credentials() const throw () {
  return (_creds);
}

/**
 *  Get the libssh2 session object.
 *
 *  @return libssh2 session object.
 */
LIBSSH2_SESSION* session::get_libssh2_session() const throw () {
  return (_session);
}

/**
 *  Get the socket handle.
 *
 *  @return Socket handle.
 */
socket_handle* session::get_socket_handle() throw () {
  return (&_socket);
}

/**
 *  Check if session is connected.
 *
 *  @return true if session is connected.
 */
bool session::is_connected() const throw () {
  return (_step == session_keepalive);
}

/**
 *  Add listener to session.
 *
 *  @param[in] listnr New listener.
 */
void session::listen(listener* listnr) {
  _listnrs.insert(listnr);
  return ;
}

/**
 *  Read data is available.
 *
 *  @param[in] h Handle.
 */
void session::read(handle& h) {
  (void)h;
  static void (session::* const redirector[])() = {
      &session::_startup,
      &session::_passwd,
      &session::_key,
      &session::_available
    };
  try {
    (this->*redirector[_step])();
  }
  catch (std::exception const& e) {
    logging::error(logging::medium) << "session "
      << _creds.get_user() << "@" << _creds.get_host()
      << " encountered an error: " << e.what();
    this->close();
  }
  return ;
}

/**
 *  Remove a listener.
 *
 *  @param[in] listnr Listener to remove.
 */
void session::unlisten(listener* listnr) {
  unsigned int size(_listnrs.size());
  std::set<listener*>::iterator it(_listnrs.find(listnr));
  if (it != _listnrs.end()) {
    if (_listnrs_it == it)
      --_listnrs_it;
    _listnrs.erase(it);
  }
  logging::debug(logging::low) << "session " << this
    << " removed listener " << listnr << " (there was "
    << size << ", there is " << _listnrs.size() << ")";
  return ;
}

/**
 *  Check if read monitoring is wanted.
 *
 *  @return true if read monitoring is wanted.
 */
bool session::want_read(handle& h) {
  (void)h;
  return (_session && (libssh2_session_block_directions(_session)
                       & LIBSSH2_SESSION_BLOCK_INBOUND));
}

/**
 *  Check if write monitoring is wanted.
 *
 *  @return true if write monitoring is wanted.
 */
bool session::want_write(handle& h) {
  (void)h;
  return (_session && (libssh2_session_block_directions(_session)
                       & LIBSSH2_SESSION_BLOCK_OUTBOUND));
}

/**
 *  Write data is available.
 *
 *  @param[in] h Handle.
 */
void session::write(handle& h) {
  read(h);
  return ;
}

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
session::session(session const& s) : com::centreon::handle_listener(s) {
  (void)s;
  assert(!"session is not copyable");
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
  assert(!"session is not copyable");
  abort();
  return (*this);
}

/**
 *  Session is available for operation.
 */
void session::_available() {
  logging::debug(logging::high) << "session " << this
    << " is available and has " << _listnrs.size() << " listeners";
  for (_listnrs_it = _listnrs.begin();
       _listnrs_it != _listnrs.end();
       ++_listnrs_it)
    (*_listnrs_it)->on_available(*this);
  return ;
}

/**
 *  Attempt public key authentication.
 */
void session::_key() {
  // Log message.
  logging::info(logging::medium)
    << "launching key-based authentication on session "
    << _creds.get_user() << "@" << _creds.get_host();

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
  int retval(libssh2_userauth_publickey_fromfile(
               _session,
               _creds.get_user().c_str(),
               pub.c_str(),
               priv.c_str(),
               _creds.get_password().c_str()));
  if (retval < 0) {
    if (retval != LIBSSH2_ERROR_EAGAIN)
      throw (basic_error() << "user authentication failed");
  }
  else {
    // Log message.
    logging::info(logging::medium)
      << "successful key-based authentication on session "
      << _creds.get_user() << "@" << _creds.get_host();

    // Enable non-blocking mode.
    libssh2_session_set_blocking(_session, 0);

    // Set execution step.
    _step = session_keepalive;
    {
      for (_listnrs_it = _listnrs.begin();
           _listnrs_it != _listnrs.end();
           ++_listnrs_it)
        (*_listnrs_it)->on_connected(*this);
    }
  }
  return ;
}

/**
 *  Try password authentication.
 */
void session::_passwd() {
  // Log message.
  logging::info(logging::medium)
    << "launching password-based authentication on session "
    << _creds.get_user() << "@" << _creds.get_host();

  // Try password.
  int retval(libssh2_userauth_password(
               _session,
               _creds.get_user().c_str(),
               _creds.get_password().c_str()));
  if (retval != 0) {
#if LIBSSH2_VERSION_NUM >= 0x010203
    if (retval == LIBSSH2_ERROR_AUTHENTICATION_FAILED) {
#else
    if ((retval != LIBSSH2_ERROR_EAGAIN)
        && (retval != LIBSSH2_ERROR_ALLOC)
        && (retval != LIBSSH2_ERROR_SOCKET_SEND)) {
#endif /* libssh2 version >= 1.2.3 */
      logging::info(logging::medium)
        << "could not authenticate with password on session "
        << _creds.get_user() << "@" << _creds.get_host();
      _step = session_key;
      _key();
    }
    else if (retval != LIBSSH2_ERROR_EAGAIN) {
      char* msg;
      libssh2_session_last_error(_session, &msg, NULL, 0);
      throw (basic_error() << "password authentication failed: "
               << msg << " (error " << retval << ")");
    }
  }
  else {
    // Log message.
    logging::info(logging::medium)
      << "successful password authentication on session "
      << _creds.get_user() << "@" << _creds.get_host();

    // We're now connected.
    _step = session_keepalive;
    {
      for (_listnrs_it = _listnrs.begin();
           _listnrs_it != _listnrs.end();
           ++_listnrs_it)
        (*_listnrs_it)->on_connected(*this);
    }
  }
  return ;
}

/**
 *  Perform SSH connection startup.
 */
void session::_startup() {
  // Log message.
  logging::info(logging::high)
    << "attempting to initialize SSH session "
    << _creds.get_user() << "@" << _creds.get_host();

  // Enable non-blocking mode.
  libssh2_session_set_blocking(_session, 0);

  // Exchange banners, keys, setup crypto, compression, ...
  int retval(libssh2_session_startup(
               _session,
               _socket.get_native_handle()));
  if (retval) {
    if (retval != LIBSSH2_ERROR_EAGAIN) { // Fatal failure.
      char* msg;
      int code(libssh2_session_last_error(_session, &msg, NULL, 0));
      throw (basic_error() << "failure establishing SSH session: "
               << msg << " (error " << code << ")");
    }
  }
  else { // Successful startup.
    // Log message.
    logging::info(logging::medium) << "SSH session "
      << _creds.get_user() << "@" << _creds.get_host()
      << " successfully initialized";

#ifdef WITH_KNOWN_HOSTS_CHECK
    // Initialize known hosts list.
    LIBSSH2_KNOWNHOSTS* known_hosts(libssh2_knownhost_init(_session));
    if (!known_hosts) {
      char* msg;
      libssh2_session_last_error(_session, &msg, NULL, 0);
      throw (basic_error()
               << "could not create known hosts list: " << msg);
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
    int rh(libssh2_knownhost_readfile(
             known_hosts,
             known_hosts_file.c_str(),
             LIBSSH2_KNOWNHOST_FILE_OPENSSH));
    if (rh < 0)
      throw (basic_error() << "parsing of known_hosts file "
               << known_hosts_file << " failed: error " << -rh);
    else
      logging::info(logging::medium) << rh
        << " hosts found in known_hosts file " << known_hosts_file;

    // Check host fingerprint against known hosts.
    logging::info(logging::high) << "checking fingerprint on session "
      << _creds.get_user() << "@" << _creds.get_host();

    // Get peer fingerprint.
    size_t len;
    int type;
    char const* fingerprint(libssh2_session_hostkey(
                              _session,
                              &len,
                              &type));
    if (!fingerprint) {
      char* msg;
      libssh2_session_last_error(_session, &msg, NULL, 0);
      libssh2_knownhost_free(known_hosts);
      throw (basic_error()
               << "failed to get remote host fingerprint: " << msg);
    }

    // Check fingerprint.
    libssh2_knownhost* kh;
#if LIBSSH2_VERSION_NUM >= 0x010206
    // Introduced in 1.2.6.
    int check(libssh2_knownhost_checkp(
                known_hosts,
                _creds.get_host().c_str(),
                -1,
                fingerprint,
                len,
                LIBSSH2_KNOWNHOST_TYPE_PLAIN
                | LIBSSH2_KNOWNHOST_KEYENC_RAW,
                &kh));
#else
      // 1.2.5 or older.
    int check(libssh2_knownhost_check(
                known_hosts,
                creds.get_host().c_str(),
                fingerprint,
                len,
                LIBSSH2_KNOWNHOST_TYPE_PLAIN
                | LIBSSH2_KNOWNHOST_KEYENC_RAW,
                &kh));
#endif // LIBSSH2_VERSION_NUM

    // Free known hosts list.
    libssh2_knownhost_free(known_hosts);

    // Check fingerprint.
    if (check != LIBSSH2_KNOWNHOST_CHECK_MATCH) {
      exceptions::basic e(basic_error());
      e << "host '" << _creds.get_host()
        << "' is not known or could not be validated: ";
      if (LIBSSH2_KNOWNHOST_CHECK_NOTFOUND == check)
        e << "host was not found in known_hosts file "
          << known_hosts_file;
      else if (LIBSSH2_KNOWNHOST_CHECK_MISMATCH == check)
        e << "host fingerprint mismatch with known_hosts file "
          << known_hosts_file;
      else
        e << "unknown error";
      throw (e);
    }
    logging::info(logging::medium) << "fingerprint on session "
      << _creds.get_user() << "@" << _creds.get_host()
      << " matches a known host";
#endif // WITH_KNOWN_HOSTS_CHECKS
    // Successful peer authentication.
    _step = session_password;
    _passwd();
  }
  return ;
}
