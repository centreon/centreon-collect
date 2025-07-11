/**
 * Copyright 2011-2013,2015 Centreon
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * For more information : contact@centreon.com
 */

#include <pwd.h>

#include "com/centreon/connector/log.hh"
#include "com/centreon/exceptions/msg_fmt.hh"

#include "com/centreon/connector/ssh/sessions/session.hh"

using namespace com::centreon;
using namespace com::centreon::connector;
using namespace com::centreon::connector::ssh::sessions;
using com::centreon::exceptions::msg_fmt;

namespace com::centreon::connector::ssh::sessions {
std::ostream& operator<<(std::ostream& os, const session& sess) {
  os << "s this:" << &sess << ' ' << sess.get_credentials();
  return os;
}

}  // namespace com::centreon::connector::ssh::sessions

namespace fmt {
// formatter specializations for fmt
template <>
struct formatter<asio::ip::tcp::endpoint> : ostream_formatter {};
}  // namespace fmt

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
session::session(credentials const& creds, const shared_io_context& io_context)
    : _creds(creds),
      _session(nullptr),
      _socket(*io_context),
      _io_context(io_context),
      _socket_waiting(false),
      _step(e_step::session_startup),
      _step_string("startup"),
      _writing(false),
      _second_timer(*io_context),
      _connect_timer(*io_context) {
  // Create session instance.
  _session = libssh2_session_init_ex(nullptr, nullptr, nullptr, this);
  if (!_session)
    throw msg_fmt("SSH session creation failed (out of memory ?)");
  libssh2_session_callback_set(_session, LIBSSH2_CALLBACK_RECV,
                               (void*)g_socket_recv);
  libssh2_session_callback_set(_session, LIBSSH2_CALLBACK_SEND,
                               (void*)g_socket_send);
  log::core()->debug("new session this:{}", *this);
}

/**
 *  Destructor.
 */
session::~session() noexcept {
  this->close();

  log::core()->debug("delete session this:{}", *this);

  // Delete session.
  libssh2_session_set_blocking(_session, 1);
  libssh2_session_disconnect(_session, "Centreon SSH Connector shutdown");
  libssh2_session_free(_session);
}

/**
 *  Close session.
 */
void session::close() {
  boost::system::error_code err;
  _socket.shutdown(asio::ip::tcp::socket::shutdown_both, err);
  _socket.close(err);
  _second_timer.cancel();
}

/****************************************************************************
 *              async
 ****************************************************************************/

/**
 * @brief execute the libssh2 function
 * if the function return another value than LIBSSH2_ERROR_EAGAIN, it calls the
 * callback handler
 *
 * @return true if the handler is called
 */
int session::ssh2_action::on_socket_exchange(bool force) {
  int ret = _action();
  if (ret != LIBSSH2_ERROR_EAGAIN) {
    return ret;
  }
  return force ? LIBSSH2_ERROR_TIMEOUT : LIBSSH2_ERROR_EAGAIN;
}

void session::notify_listeners(bool force) {
  for (async_list::iterator notif_iter = _async_listeners.begin();
       notif_iter != _async_listeners.end();) {
    ssh2_action::pointer action = *notif_iter;
    int retval = action->on_socket_exchange(force);
    if (retval != LIBSSH2_ERROR_EAGAIN) {
      notif_iter = _async_listeners.erase(notif_iter);
      action->call_callback(retval);
    } else {
      ++notif_iter;
    }
  }
}

void session::start_second_timer() {
  _second_timer.expires_after(std::chrono::seconds(1));
  _second_timer.async_wait(
      [me = shared_from_this()](const boost::system::error_code& err) {
        if (!err) {
          time_point now(system_clock::now());
          for (async_list::iterator notif_iter = me->_async_listeners.begin();
               notif_iter != me->_async_listeners.end();) {
            if ((*notif_iter)->get_time_out() < now) {
              ssh2_action::pointer to_call = *notif_iter;
              notif_iter = me->_async_listeners.erase(notif_iter);
              if (notif_iter == me->_async_listeners.end()) {
                to_call->call_callback(to_call->on_socket_exchange(true));
                return;
              } else {
                to_call->call_callback(to_call->on_socket_exchange(true));
              }
            } else {
              ++notif_iter;
            }
          }
          me->start_second_timer();
        }
      });
}

/****************************************************************************
 *              connect
 ****************************************************************************/
/**
 *  Open session.
 */
void session::connect(connect_callback callback, const time_point& timeout) {
  // Check that session wasn't already open.
  if (_step != e_step::session_startup) {
    log::core()->info("attempt to open already opened session");
    return;
  }

  // Step.
  _step_string = "startup";
  std::shared_ptr<asio::ip::tcp::resolver> resolver(
      std::make_shared<asio::ip::tcp::resolver>(*_io_context));

  log::core()->debug("resolve:{}", *this);
  resolver->async_resolve(
      _creds.get_host(), std::to_string(_creds.get_port()),
      [me = shared_from_this(), callback, resolver, timeout](
          const boost::system::error_code& err,
          const asio::ip::tcp::resolver::results_type& results) {
        me->on_resolve(err, results, callback, timeout);
      });

  _connect_timer.expires_at(timeout);
  _connect_timer.async_wait([me = shared_from_this(),
                             resolver](const boost::system::error_code& err) {
    if (!err) {
      log::core()->error("time out connect {}", *me);
      boost::system::error_code e;
      me->_socket.cancel(e);
      resolver->cancel();
    }
  });
}

static const boost::system::error_code host_not_found(
    asio::error::host_not_found,
    asio::error::get_netdb_category());

void session::on_resolve(const boost::system::error_code& error,
                         const asio::ip::tcp::resolver::results_type& results,
                         connect_callback callback,
                         const time_point& timeout) {
  if (error) {
    _connect_timer.cancel();
    log::core()->error("resolver error for {} : {}", _creds, error.message());
    callback(error);
    return;
  }
  if (system_clock::now() >= timeout) {
    log::core()->error("resolver timeout for {}", _creds);
    callback(std::make_error_code(std::errc::timed_out));
    return;
  }

  if (results.empty()) {
    _connect_timer.cancel();
    log::core()->error("host not found for {} : {}", _creds, error.message());
    callback(error);
    return;
  }

  asio::ip::tcp::resolver::results_type::const_iterator current =
      results.begin();
  log::core()->debug("{} connect to {}", *this, current->endpoint());
  _socket.async_connect(
      current->endpoint(), [me = shared_from_this(), current, results, callback,
                            timeout](const boost::system::error_code& err) {
        me->on_connect(err, current, results, callback, timeout);
      });
}

void session::on_connect(
    const boost::system::error_code& error,
    asio::ip::tcp::resolver::results_type::const_iterator current_endpoint,
    const asio::ip::tcp::resolver::results_type& all_res,
    connect_callback callback,
    const time_point& timeout) {
  if (system_clock::now() >= timeout) {
    log::core()->error("connect timeout for {}", _creds);
    callback(std::make_error_code(std::errc::timed_out));
    return;
  }

  if (error) {
    log::core()->error("fail to connect to {}", current_endpoint->endpoint());
    ++current_endpoint;
    if (current_endpoint != all_res.end()) {
      log::core()->debug("{} connect to {}", *this,
                         current_endpoint->endpoint());
      _socket.async_connect(
          current_endpoint->endpoint(),
          [me = shared_from_this(), current_endpoint, all_res, callback,
           timeout](const boost::system::error_code& err) {
            me->on_connect(err, current_endpoint, all_res, callback, timeout);
          });
    } else {
      _connect_timer.cancel();
      callback(error);
    }
  } else {
    _connect_timer.cancel();
    log::core()->debug("{} connected to {}", *this,
                       current_endpoint->endpoint());
    start_read();
    _startup(callback, timeout);
  }
}

/****************************************************************************
 *              socket recv
 ****************************************************************************/
void session::start_read() {
  try {
    recv_data::pointer recv_buff(std::make_shared<recv_data>());
    _socket.async_receive(asio::buffer(recv_buff->buff),
                          [me = shared_from_this(), recv_buff](
                              const boost::system::error_code& error,
                              std::size_t bytes_transferred) {
                            me->read_handler(error, recv_buff,
                                             bytes_transferred);
                          });
  } catch (const std::exception& e) {
    log::core()->error("fail to async_read from {} : {}", _creds, e.what());
    error();
  }
}

void session::read_handler(const boost::system::error_code& err,
                           const recv_data::pointer& buff,
                           size_t nb_recv) {
  if (err) {
    log::core()->error("read fail from {} : {}", _creds, err.message());
    error();
    return;
  }
  log::core()->debug("{} {} bytes received", *this, nb_recv);

  if (nb_recv > 0) {
    buff->size = nb_recv;
    _recv_queue.push(buff);
    notify_listeners(false);
  }
  start_read();
}

ssize_t session::g_socket_recv(libssh2_socket_t sockfd,
                               void* buffer,
                               size_t length,
                               int,
                               void** abstract) {
  session* session_obj = static_cast<session*>(*abstract);
  return session_obj->socket_recv(sockfd, buffer, length);
}

ssize_t session::socket_recv(libssh2_socket_t sockfd,
                             void* buffer,
                             size_t length) {
  if (!_socket.is_open()) {
    return -1;
  }
  assert(sockfd == _socket.native_handle());
  size_t copied = 0;
  while (copied < length && !_recv_queue.empty()) {
    recv_data::pointer to_copy = _recv_queue.front();
    size_t nb_to_copy =
        std::min(length - copied, to_copy->size - to_copy->offset);
    memcpy(buffer, to_copy->buff.data() + to_copy->offset, nb_to_copy);
    copied += nb_to_copy;
    to_copy->offset += nb_to_copy;
    if (to_copy->offset >= to_copy->size) {
      _recv_queue.pop();
    }
  }

  log::core()->debug("{} read {} bytes ", *this, copied);

  return copied ? copied : -EAGAIN;
}

/****************************************************************************
 *              socket send
 ****************************************************************************/
session::send_data::send_data(const void* data, size_t size)
    : _buff(new unsigned char[size]), _size(size) {
  memcpy(_buff, data, size);
}

session::send_data::~send_data() {
  delete[] _buff;
}

ssize_t session::g_socket_send(libssh2_socket_t sockfd,
                               const void* buffer,
                               size_t length,
                               int,
                               void** abstract) {
  session* session_obj = static_cast<session*>(*abstract);
  return session_obj->socket_send(sockfd, buffer, length);
}

ssize_t session::socket_send(libssh2_socket_t sockfd,
                             const void* buffer,
                             size_t length) {
  if (!_socket.is_open()) {
    return -1;
  }
  assert(sockfd == _socket.native_handle());

  log::core()->debug("{} want to send {} bytes ", *this, length);

  _send_queue.emplace(std::make_shared<send_data>(buffer, length));

  start_send();
  return length;
}

void session::start_send() {
  if (_writing || _send_queue.empty()) {
    return;
  }
  try {
    send_data::pointer tosend = _send_queue.front();
    _send_queue.pop();
    asio::async_write(_socket,
                      asio::buffer(tosend->get_buff(), tosend->get_size()),
                      [me = shared_from_this(), tosend](
                          const boost::system::error_code& err,
                          size_t nb_sent) { me->send_handler(err, nb_sent); });
  } catch (const std::exception& e) {
    log::core()->error("fail to async_write to {} : {}", _creds, e.what());
    close();
    return;
  }
  _writing = true;
}

void session::send_handler(const boost::system::error_code& err,
                           size_t nb_sent) {
  if (err) {
    log::core()->error("fail to send to {} : {}", _creds, err.message());
    close();
    return;
  }
  log::core()->debug("{} {} bytes sent", *this, nb_sent);

  _writing = false;
  start_send();
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
  log::core()->error(
      "error detected on socket, shutting down session {0}@{1}:{2}",
      _creds.get_user(), _creds.get_host(), _creds.get_port());
  close();

  asio::post(*_io_context,
             [me = shared_from_this()]() { me->notify_listeners(true); });

  _step = e_step::session_error;
  _step_string = "error";
}

/**
 *  Get a new channel.
 *
 *  @return New channel if possible, nullptr otherwise.
 */
int session::new_channel(LIBSSH2_CHANNEL*& chan) {
  // Attempt to open channel.
  chan = libssh2_channel_open_session(_session);

  // Channel creation failed, check that we can try again later.
  if (chan) {
    return 0;
  }
  char* msg;
  return libssh2_session_last_error(_session, &msg, nullptr, 0);
}

/****************************************************************************
 *              authentication
 ****************************************************************************/

/**
 *  Attempt public key authentication.
 */
void session::_key(connect_callback callback, const time_point& timeout) {
  // Log message.
  log::core()->info("launching key-based authentication on session {}", _creds);

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

  async_wait(
      [me = shared_from_this(), this, pub, priv]() {
        return libssh2_userauth_publickey_fromfile(
            _session, _creds.get_user().c_str(), pub.c_str(), priv.c_str(),
            _creds.get_password().c_str());
      },
      [me = shared_from_this(), callback, pub](int retval) {
        if (retval) {
          char* msg;
          libssh2_session_last_error(me->_session, &msg, nullptr, 0);
          log::core()->error("fail to authent with {} for {} {}:{}", pub,
                             me->_creds, retval, msg);
          callback(std::make_error_code(std::errc::permission_denied));
        } else {
          log::core()->info("successful key-based authentication on session {}",
                            me->_creds);

          // Set execution step.
          me->_step = e_step::session_keepalive;
          me->_step_string = "keep-alive";
          callback({});
          me->start_second_timer();
        }
      },
      timeout, "session::_key");
}

/**
 *  Try password authentication.
 */
void session::_passwd(connect_callback callback, const time_point& timeout) {
  // Log message.
  log::core()->info("launching password-based authentication on session {}",
                    _creds);

  // Try password.
  async_wait(
      [me = shared_from_this(), this]() {
        return libssh2_userauth_password(_session, _creds.get_user().c_str(),
                                         _creds.get_password().c_str());
      },
      [me = shared_from_this(), callback, timeout](int ret) {
        me->_passwd_handler(ret, callback, timeout);
      },
      timeout, "session::_passwd");
}

void session::_passwd_handler(int retval,
                              connect_callback callback,
                              const time_point& timeout) {
  if (retval != 0) {
    if (retval == LIBSSH2_ERROR_AUTHENTICATION_FAILED) {
      log::core()->info(
          "could not authenticate with password on session {}: {}", _creds,
          retval);
      _step = e_step::session_key;
      _step_string = "public key authentication";
      _key(callback, timeout);
    } else {
      char* msg;
      libssh2_session_last_error(_session, &msg, nullptr, 0);
      callback(std::make_error_code(std::errc::permission_denied));
    }
  } else {
    // Log message.
    log::core()->info("successful password authentication on session {}",
                      _creds);

    // We're now connected.
    _step = e_step::session_keepalive;
    _step_string = "keep-alive";
    callback({});
    start_second_timer();
  }
}

static bool activate_keep_alive(int s_interval, int fd) {
  int flag = 1;
  if (setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, (const void*)&flag,
                 sizeof(flag))) {
    log::core()->error("fail to activate keep alive on socket {}", fd);
    return false;
  }
  if (setsockopt(fd, SOL_TCP, TCP_KEEPINTVL, (const void*)&s_interval,
                 sizeof(s_interval))) {
    log::core()->error("fail to fix keep alive interval on socket {}", fd);
    return false;
  }
  if (setsockopt(fd, SOL_TCP, TCP_KEEPIDLE, (const void*)&s_interval,
                 sizeof(s_interval))) {
    log::core()->error("fail to fix keep alive idle on socket {}", fd);
    return false;
  }

  return true;
}

/**
 *  Perform SSH connection startup.
 */
void session::_startup(connect_callback callback, const time_point& timeout) {
  // activate 30s tcp keep alive
  activate_keep_alive(30, _socket.native_handle());

  // Log message.
  log::core()->info("attempting to initialize SSH session {}", _creds);

  // Enable non-blocking mode.
  libssh2_session_set_blocking(_session, 0);
  handshake(callback, timeout);
}

void session::handshake(connect_callback callback, const time_point& timeout) {
  log::core()->info("handshake {}", _creds);
  async_wait(
      [me = shared_from_this(), this]() {
        return libssh2_session_handshake(_session, _socket.native_handle());
      },
      [me = shared_from_this(), callback, timeout](int ret) {
        me->handshake_handler(ret, callback, timeout);
      },
      timeout, "session::handshake");
}

void session::handshake_handler(int retval,
                                connect_callback callback,
                                const time_point& timeout) {
  if (retval) {
    char* msg;
    int code(libssh2_session_last_error(_session, &msg, nullptr, 0));
    log::core()->error("fail handshake with {} ret_code={} {}", _creds, code,
                       msg);
    callback(std::make_error_code(std::errc::io_error));
  } else {  // Successful startup.
    // Log message.
    log::core()->info("SSH session {} successfully initialized", _creds);

#ifdef WITH_KNOWN_HOSTS_CHECK
    // Initialize known hosts list.
    LIBSSH2_KNOWNHOSTS* known_hosts(libssh2_knownhost_init(_session));
    if (!known_hosts) {
      char* msg;
      libssh2_session_last_error(_session, &msg, nullptr, 0);
      throw msg_fmt("could not create known hosts list: {}", msg);
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
    int rh(libssh2_knownhost_readfile(known_hosts, known_hosts_file.c_str(),
                                      LIBSSH2_KNOWNHOST_FILE_OPENSSH));
    if (rh < 0)
      throw msg_fmt("parsing of known_hosts file {}  failed: error {}",
                    known_hosts_file, -rh);
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
      throw msg_fmt("failed to get remote host fingerprint: {}", msg);
    }

    // Check fingerprint.
    libssh2_knownhost* kh;
    int check(libssh2_knownhost_checkp(
        known_hosts, _creds.get_host().c_str(), -1, fingerprint, len,
        LIBSSH2_KNOWNHOST_TYPE_PLAIN | LIBSSH2_KNOWNHOST_KEYENC_RAW, &kh));

    // Free known hosts list.
    libssh2_knownhost_free(known_hosts);

    // Check fingerprint.
    if (check != LIBSSH2_KNOWNHOST_CHECK_MATCH) {
      if (LIBSSH2_KNOWNHOST_CHECK_NOTFOUND == check)
        throw msg_fmt(
            "host '{}' is not known or could not be validated: host was not "
            "found in known_hosts file {}",
            _creds.get_host(), known_hosts_file);
      else if (LIBSSH2_KNOWNHOST_CHECK_MISMATCH == check)
        throw msg_fmt(
            "host '{}' is not known or could not be validated: host "
            "fingerprint mismatch with known_hosts file {}",
            _creds.get_host(), known_hosts_file);
      else
        throw msg_fmt(
            "host '{}' is not known or could not be validated: unknown error",
            _creds.get_host());
    }
    log_info(logging::medium) << "fingerprint on session " << _creds.get_user()
                              << "@" << _creds.get_host() << ":"
                              << _creds.get_port() << " matches a known host";
#endif  // WITH_KNOWN_HOSTS_CHECKS
    // Successful peer authentication.
    _step = e_step::session_password;
    _step_string = "password authentication";
    _passwd(callback, timeout);
  }
}