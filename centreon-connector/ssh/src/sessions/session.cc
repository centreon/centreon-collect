/*
** Copyright 2011-2013,2015 Centreon
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

#include "com/centreon/connector/ssh/sessions/session.hh"
#include <arpa/inet.h>
#include <fcntl.h>
#include <libssh2.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pwd.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <memory>
#include "com/centreon/connector/ssh/multiplexer.hh"
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
      _needed_new_chan(false),
      _session(nullptr),
      _step(session_startup),
      _step_string("startup") {
  // Create session instance.
  _session = libssh2_session_init();
  if (!_session)
    throw basic_error() << "SSH session creation failed (out of memory ?)";
}

/**
 *  Destructor.
 */
session::~session() noexcept {
  try {
    this->close();
  } catch (...) {
  }

  // Delete session.
  libssh2_session_set_blocking(_session, 1);
  libssh2_session_disconnect(_session, "Centreon SSH Connector shutdown");
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
    for (auto& l : _listnrs)
      l->on_close(*this);
  }

  // Close socket.
  _socket.close();
}

/**
 *  Open session.
 */
void session::connect(bool use_ipv6) {
  // Check that session wasn't already open.
  if (is_connected()) {
    log_info(logging::high) << "attempt to open already opened session";
    return;
  }

  // Step.
  _step = session_startup;
  _step_string = "startup";

  char const* host_ptr(_creds.get_host().c_str());
  unsigned short port(_creds.get_port());

  // Host lookup.
  log_info(logging::high) << "looking up address " << host_ptr;
  int ret;
  int sin_size;
  std::unique_ptr<sockaddr> sin;
  if (use_ipv6) {
    sockaddr_in6* sin6(new sockaddr_in6);
    sin = std::unique_ptr<sockaddr>((sockaddr*)(sin6));
    sin_size = sizeof(*sin6);
    memset(sin6, 0, sin_size);

    // Set address info.
    sin6->sin6_family = AF_INET6;
    sin6->sin6_port = htons(port);

    // Try to avoid DNS lookup.
    ret = inet_pton(AF_INET6, host_ptr, &sin6->sin6_addr);
  } else {
    sockaddr_in* sin4(new sockaddr_in);
    sin = std::unique_ptr<sockaddr>((sockaddr*)(sin4));
    sin_size = sizeof(*sin4);
    memset(sin4, 0, sin_size);

    // Set address info.
    sin4->sin_family = AF_INET;
    sin4->sin_port = htons(port);

    // Try to avoid DNS lookup.
    ret = inet_pton(AF_INET, host_ptr, &sin4->sin_addr);
  }

  if (ret == 1)
    log_debug(logging::high) << "host " << host_ptr << " is an IP address";
  // DNS lookup.
  else {
    addrinfo hint;
    memset(&hint, 0, sizeof(hint));
    hint.ai_family = (use_ipv6 ? AF_INET6 : AF_INET);
    hint.ai_socktype = SOCK_STREAM;
    addrinfo* res(nullptr);
    int retval(getaddrinfo(host_ptr, nullptr, &hint, &res));
    if (retval)
      throw basic_error() << "lookup of host '" << host_ptr
                          << "' failed: " << gai_strerror(retval);
    else if (!res)
      throw basic_error() << "no IPv" << (use_ipv6 ? "6" : "4")
                          << " address found for host '" << host_ptr << "'";

    // Log message.
    log_debug(logging::low)
        << "found host " << host_ptr << " address through name resolution";

    // Get address.
    if (use_ipv6)
      ((sockaddr_in6*)sin.get())->sin6_addr =
          ((sockaddr_in6*)(res->ai_addr))->sin6_addr;
    else
      ((sockaddr_in*)sin.get())->sin_addr.s_addr =
          ((sockaddr_in*)(res->ai_addr))->sin_addr.s_addr;
    // Free result.
    freeaddrinfo(res);
  }

  // Create socket.
  int mysocket;
  mysocket = ::socket((use_ipv6 ? AF_INET6 : AF_INET), SOCK_STREAM, 0);
  if (mysocket < 0) {
    char const* msg(strerror(errno));
    throw basic_error() << "socket creation failed: " << msg;
  }

  // Set socket non-blocking.
  int flags(fcntl(mysocket, F_GETFL));
  if (flags < 0) {
    char const* msg(strerror(errno));
    ::close(mysocket);
    throw basic_error() << "could not get socket flags: " << msg;
  }
  flags |= O_NONBLOCK;
  if (fcntl(mysocket, F_SETFL, flags) == -1) {
    char const* msg(strerror(errno));
    ::close(mysocket);
    throw basic_error() << "could not make socket non blocking: " << msg;
  }

  // Connect to remote host.
  if ((::connect(mysocket, sin.get(), sin_size) != 0) &&
      (errno != EINPROGRESS)) {
    char const* msg(strerror(errno));
    ::close(mysocket);
    throw basic_error() << "could not connect to '" << host_ptr << "': " << msg;
  }

  _socket.set_native_handle(mysocket);

  // Register with multiplexer.
  multiplexer::instance().handle_manager::add(&_socket, this, true);

  // Launch the connection process.
  log_debug(logging::medium)
      << "manually launching the connection process of session "
      << _creds.get_user() << "@" << _creds.get_host() << ":"
      << _creds.get_port();
  _startup();
}

/**
 *  @brief Set session in error.
 *
 *  This method is usually called by channels on I/O errors to make the
 *  session immediately aware that it is not valid anymore. Otherwise we
 *  would have to wait an extra cycle to detect this I/O error (when
 *  another channel is requested).
 */
void session::error() {
  _step = session_error;
  _step_string = "error";
}

/**
 *  Error callback (from I/O multiplexing).
 *
 *  @param[in,out] h Handle.
 */
void session::error(handle& h) {
  (void)h;
  log_error(logging::low) << "error detected on socket, shutting down session "
                          << _creds.get_user() << "@" << _creds.get_host()
                          << ":" << _creds.get_port();
  this->close();
}

/**
 *  Get the session credentials.
 *
 *  @return Credentials associated to this session.
 */
credentials const& session::get_credentials() const noexcept { return _creds; }

/**
 *  Get the libssh2 session object.
 *
 *  @return libssh2 session object.
 */
LIBSSH2_SESSION* session::get_libssh2_session() const noexcept {
  return _session;
}

/**
 *  Get the socket handle.
 *
 *  @return Socket handle.
 */
socket_handle* session::get_socket_handle() noexcept {
  return &_socket;
}

/**
 *  Check if session is connected.
 *
 *  @return true if session is connected.
 */
bool session::is_connected() const noexcept {
  return _step == session_keepalive;
}

/**
 *  Add listener to session.
 *
 *  @param[in] listnr New listener.
 */
void session::listen(listener* listnr) {
  _listnrs.insert(listnr);
}

/**
 *  Get a new channel.
 *
 *  @return New channel if possible, nullptr otherwise.
 */
LIBSSH2_CHANNEL* session::new_channel() {
  // Return value.
  LIBSSH2_CHANNEL* chan;

  // New channel flag.
  _needed_new_chan = true;

  // Attempt to open channel.
  chan = libssh2_channel_open_session(_session);

  // Channel creation failed, check that we can try again later.
  if (!chan) {
    char* msg;
    int ret(libssh2_session_last_error(_session, &msg, nullptr, 0));
    if (ret != LIBSSH2_ERROR_EAGAIN) {
      if (ret == LIBSSH2_ERROR_SOCKET_SEND)
        error();
      throw basic_error() << "could not open SSH channel: " << msg;
    }
  }

  // Return channel.
  return chan;
}

/**
 *  Read available data.
 *
 *  @param[in] h Handle.
 */
void session::read(handle& h) {
  (void)h;
  static void (session::*const redirector[])() = {
      &session::_startup, &session::_passwd,
      &session::_key,     &session::_available};

  try {
    (this->*redirector[_step])();
  } catch (std::exception const& e) {
    log_error(logging::medium)
        << "session " << _creds.get_user() << "@" << _creds.get_host() << ":"
        << _creds.get_port() << " encountered an error: " << e.what();
    this->close();
  }
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
      ++_listnrs_it;
    _listnrs.erase(it);
  }
  log_debug(logging::low) << "session " << this << " removed listener "
                          << listnr << " (there was " << size << ", there is "
                          << _listnrs.size() << ")";
}

/**
 *  Check if read monitoring is wanted.
 *
 *  @return true if read monitoring is wanted.
 */
bool session::want_read(handle& h) {
  (void)h;
  bool retval(_session && (libssh2_session_block_directions(_session) &
                           LIBSSH2_SESSION_BLOCK_INBOUND));
  log_debug(logging::low) << "session " << _creds.get_user() << "@"
                          << _creds.get_host() << ":" << _creds.get_port()
                          << (retval ? "" : " do not") << " want to read (step "
                          << _step_string << ")";
  return retval;
}

/**
 *  Check if write monitoring is wanted.
 *
 *  @return true if write monitoring is wanted.
 */
bool session::want_write(handle& h) {
  (void)h;
  bool retval(_session && ((libssh2_session_block_directions(_session) &
                            LIBSSH2_SESSION_BLOCK_OUTBOUND) ||
                           _needed_new_chan));
  log_debug(logging::low) << "session " << _creds.get_user() << "@"
                          << _creds.get_host() << ":" << _creds.get_port()
                          << (retval ? "" : " do not")
                          << " want to write (step " << _step_string << ")";
  return retval;
}

/**
 *  Write data is available.
 *
 *  @param[in] h Handle.
 */
void session::write(handle& h) {
  _needed_new_chan = false;
  read(h);
}

/**************************************
 *                                     *
 *           Private Methods           *
 *                                     *
 **************************************/

/**
 *  Session is available for operation.
 */
void session::_available() {
  log_debug(logging::high) << "session " << this << " is available and has "
                           << _listnrs.size() << " listeners";
  for (auto& l : _listnrs)
    l->on_available(*this);
}

/**
 *  Attempt public key authentication.
 */
void session::_key() {
  // Log message.
  log_info(logging::medium)
      << "launching key-based authentication on session " << _creds.get_user()
      << "@" << _creds.get_host() << ":" << _creds.get_port();

  // Build key paths.
  std::string priv;
  std::string pub;

  if (_creds.get_key().empty()) {
    // Get home directory.
    passwd* pw(getpwuid(getuid()));
    if (pw && pw->pw_dir) {
      priv = pw->pw_dir;
      priv.append("/");
      pub = pw->pw_dir;
      pub.append("/");
    }
    priv.append(".ssh/id_rsa");
    pub.append(".ssh/id_rsa.pub");
  } else {
    priv = _creds.get_key();
    pub = priv + ".pub";
  }

  // Try public key authentication.
  int retval(libssh2_userauth_publickey_fromfile(
      _session, _creds.get_user().c_str(), pub.c_str(), priv.c_str(),
      _creds.get_password().c_str()));
  if (retval < 0) {
    if (retval != LIBSSH2_ERROR_EAGAIN)
      throw basic_error() << "user authentication failed";
  } else {
    // Log message.
    log_info(logging::medium)
        << "successful key-based authentication on session "
        << _creds.get_user() << "@" << _creds.get_host() << ":"
        << _creds.get_port();

    // Enable non-blocking mode.
    libssh2_session_set_blocking(_session, 0);

    // Set execution step.
    _step = session_keepalive;
    _step_string = "keep-alive";
    {
      for (auto& l : _listnrs)
        l->on_connected(*this);
    }
  }
}

/**
 *  Try password authentication.
 */
void session::_passwd() {
  // Log message.
  log_info(logging::medium)
      << "launching password-based authentication on session "
      << _creds.get_user() << "@" << _creds.get_host() << ":"
      << _creds.get_port();

  // Try password.
  int retval(libssh2_userauth_password(
      _session, _creds.get_user().c_str(), _creds.get_password().c_str()));
  if (retval != 0) {
#if LIBSSH2_VERSION_NUM >= 0x010203
    if (retval == LIBSSH2_ERROR_AUTHENTICATION_FAILED) {
#else
    if ((retval != LIBSSH2_ERROR_EAGAIN) && (retval != LIBSSH2_ERROR_ALLOC) &&
        (retval != LIBSSH2_ERROR_SOCKET_SEND)) {
#endif /* libssh2 version >= 1.2.3 */
      log_info(logging::medium)
          << "could not authenticate with password on session "
          << _creds.get_user() << "@" << _creds.get_host() << ":"
          << _creds.get_port();
      _step = session_key;
      _step_string = "public key authentication";
      _key();
    } else if (retval != LIBSSH2_ERROR_EAGAIN) {
      char* msg;
      libssh2_session_last_error(_session, &msg, nullptr, 0);
      throw basic_error() << "password authentication failed: " << msg
                          << " (error " << retval << ")";
    }
  } else {
    // Log message.
    log_info(logging::medium)
        << "successful password authentication on session " << _creds.get_user()
        << "@" << _creds.get_host() << ":" << _creds.get_port();

    // We're now connected.
    _step = session_keepalive;
    _step_string = "keep-alive";
    {
      for (auto& l : _listnrs)
        l->on_connected(*this);
    }
  }
}

/**
 *  Perform SSH connection startup.
 */
void session::_startup() {
  // Log message.
  log_info(logging::high) << "attempting to initialize SSH session "
                          << _creds.get_user() << "@" << _creds.get_host()
                          << ":" << _creds.get_port();

  // Enable non-blocking mode.
  libssh2_session_set_blocking(_session, 0);

  // Exchange banners, keys, setup crypto, compression, ...
#if LIBSSH2_VERSION_NUM >= 0x010208
  // libssh2_session_startup deprecated in version 1.2.8 and later
  int retval(libssh2_session_handshake(_session, _socket.get_native_handle()));
#else
  int retval(libssh2_session_startup(_session, _socket.get_native_handle()));
#endif
  if (retval) {
    if (retval != LIBSSH2_ERROR_EAGAIN) {  // Fatal failure.
      char* msg;
      int code(libssh2_session_last_error(_session, &msg, nullptr, 0));
      throw basic_error() << "failure establishing SSH session: " << msg
                          << " (error " << code << ")";
    }
  } else {  // Successful startup.
    // Log message.
    log_info(logging::medium)
        << "SSH session " << _creds.get_user() << "@" << _creds.get_host()
        << ":" << _creds.get_port() << " successfully initialized";

#ifdef WITH_KNOWN_HOSTS_CHECK
    // Initialize known hosts list.
    LIBSSH2_KNOWNHOSTS* known_hosts(libssh2_knownhost_init(_session));
    if (!known_hosts) {
      char* msg;
      libssh2_session_last_error(_session, &msg, nullptr, 0);
      throw basic_error() << "could not create known hosts list: " << msg;
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
        known_hosts, known_hosts_file.c_str(), LIBSSH2_KNOWNHOST_FILE_OPENSSH));
    if (rh < 0)
      throw basic_error() << "parsing of known_hosts file " << known_hosts_file
                          << " failed: error " << -rh;
    else
      log_info(logging::medium)
          << rh << " hosts found in known_hosts file " << known_hosts_file;

    // Check host fingerprint against known hosts.
    log_info(logging::high)
        << "checking fingerprint on session " << _creds.get_user() << "@"
        << _creds.get_host() << ":" << _creds.get_port();

    // Get peer fingerprint.
    size_t len;
    int type;
    char const* fingerprint(libssh2_session_hostkey(_session, &len, &type));
    if (!fingerprint) {
      char* msg;
      libssh2_session_last_error(_session, &msg, nullptr, 0);
      libssh2_knownhost_free(known_hosts);
      throw basic_error() << "failed to get remote host fingerprint: " << msg;
    }

    // Check fingerprint.
    libssh2_knownhost* kh;
#if LIBSSH2_VERSION_NUM >= 0x010206
    // Introduced in 1.2.6.
    int check(libssh2_knownhost_checkp(
        known_hosts, _creds.get_host().c_str(), -1, fingerprint, len,
        LIBSSH2_KNOWNHOST_TYPE_PLAIN | LIBSSH2_KNOWNHOST_KEYENC_RAW, &kh));
#else
    // 1.2.5 or older.
    int check(libssh2_knownhost_check(
        known_hosts, creds.get_host().c_str(), fingerprint, len,
        LIBSSH2_KNOWNHOST_TYPE_PLAIN | LIBSSH2_KNOWNHOST_KEYENC_RAW, &kh));
#endif  // LIBSSH2_VERSION_NUM

    // Free known hosts list.
    libssh2_knownhost_free(known_hosts);

    // Check fingerprint.
    if (check != LIBSSH2_KNOWNHOST_CHECK_MATCH) {
      exceptions::basic e(basic_error());
      e << "host '" << _creds.get_host()
        << "' is not known or could not be validated: ";
      if (LIBSSH2_KNOWNHOST_CHECK_NOTFOUND == check)
        e << "host was not found in known_hosts file " << known_hosts_file;
      else if (LIBSSH2_KNOWNHOST_CHECK_MISMATCH == check)
        e << "host fingerprint mismatch with known_hosts file "
          << known_hosts_file;
      else
        e << "unknown error";
      throw e;
    }
    log_info(logging::medium) << "fingerprint on session " << _creds.get_user()
                              << "@" << _creds.get_host() << ":"
                              << _creds.get_port() << " matches a known host";
#endif  // WITH_KNOWN_HOSTS_CHECKS
    // Successful peer authentication.
    _step = session_password;
    _step_string = "password authentication";
    _passwd();
  }
}
