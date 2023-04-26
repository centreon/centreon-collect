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

#include "com/centreon/connector/ssh/namespace.hh"
#include "com/centreon/connector/ssh/sessions/credentials.hh"

CCCS_BEGIN()

namespace sessions {
/**
 *  @class session session.hh "com/centreon/connector/ssh/session.hh"
 *  @brief SSH session.
 *
 *  SSH session between Centreon SSH Connector and a remote
 *  host. The session is kept open as long as needed.
 */
class session : public std::enable_shared_from_this<session> {
 public:
  enum class e_step {
    session_startup = 0,
    session_password,
    session_key,
    session_keepalive,
    session_error
  };

  using pointer = std::shared_ptr<session>;
  using connect_callback = std::function<void(const std::error_code&)>;

  session(credentials const& creds, const shared_io_context& io_context);
  ~session() noexcept;
  session(session const& s) = delete;
  session& operator=(session const& s) = delete;
  void close();
  void connect(connect_callback callback, const time_point& timeout);
  void error();
  credentials const& get_credentials() const noexcept { return _creds; };
  LIBSSH2_SESSION* get_libssh2_session() const noexcept { return _session; };
  int new_channel(LIBSSH2_CHANNEL*&);

  template <class action_type, class callback_type>
  void async_wait(action_type&& action,
                  callback_type&& callback,
                  const time_point& time_out,
                  const char* debug_info);

  shared_io_context get_io_context() const { return _io_context; }

  e_step get_state() const { return _step; }

 private:
  struct recv_data {
    using pointer = std::shared_ptr<recv_data>;

    recv_data() : offset(0), size(0) {}

    size_t offset;
    size_t size;
    static constexpr unsigned buff_size = 65536;
    std::array<u_char, buff_size> buff;
  };

  class send_data {
    unsigned char* _buff;
    size_t _size;

    send_data(const send_data&) = delete;
    send_data& operator=(const send_data&) = delete;

   public:
    ~send_data();
    using pointer = std::shared_ptr<send_data>;
    send_data(const void* data, size_t size);

    const unsigned char* get_buff() const { return _buff; }
    size_t get_size() const { return _size; }
  };

  class ssh2_action {
   public:
    using pointer = std::shared_ptr<ssh2_action>;
    using ssh2_function = std::function<int()>;
    using ssh2_function_callback = std::function<void(int)>;

   protected:
    ssh2_function _action;
    ssh2_function_callback _callback;
    time_point _time_out;
    const char* _debug_info;

   public:
    template <class action_type, class callback_type>
    ssh2_action(action_type&& action,
                callback_type&& callback,
                time_point time_out,
                const char* debug_info)
        : _action(action),
          _callback(callback),
          _time_out(time_out),
          _debug_info(debug_info) {}

    const time_point& get_time_out() const { return _time_out; }

    int on_socket_exchange(bool force);
    void call_callback(int retval) { _callback(retval); }
  };

  void on_resolve(const std::error_code& error,
                  const asio::ip::tcp::resolver::results_type& results,
                  connect_callback callback,
                  const time_point& timeout);

  void on_connect(
      const std::error_code& error,
      asio::ip::tcp::resolver::results_type::const_iterator current_endpoint,
      const asio::ip::tcp::resolver::results_type& all_res,
      connect_callback callback,
      const time_point& timeout);

  static ssize_t g_socket_recv(libssh2_socket_t sockfd,
                               void* buffer,
                               size_t length,
                               int flags,
                               void** abstract);

  ssize_t socket_recv(libssh2_socket_t sockfd, void* buffer, size_t length);
  void start_read();
  void read_handler(const std::error_code& err,
                    const recv_data::pointer& buff,
                    size_t nb_recv);

  static ssize_t g_socket_send(libssh2_socket_t sockfd,
                               const void* buffer,
                               size_t length,
                               int flags,
                               void** abstract);

  ssize_t socket_send(libssh2_socket_t sockfd,
                      const void* buffer,
                      size_t length);

  void start_send();
  void send_handler(const std::error_code& err, size_t nb_sent);

  void notify_listeners(bool force);

  void handshake(connect_callback callback, const time_point& timeout);
  void handshake_handler(int ret,
                         connect_callback callback,
                         const time_point& timeout);
  void _key(connect_callback callback, const time_point& timeout);
  void _key_handler(int retval,
                    connect_callback callback,
                    const time_point& timeout);
  void _passwd(connect_callback callback, const time_point& timeout);
  void _passwd_handler(int ret,
                       connect_callback callback,
                       const time_point& timeout);
  void _startup(connect_callback callback, const time_point& timeout);

  void start_second_timer();

  const credentials _creds;
  LIBSSH2_SESSION* _session;
  asio::ip::tcp::socket _socket;
  shared_io_context _io_context;
  using async_listener = std::function<void()>;
  using listener_cont = std::vector<async_listener>;
  listener_cont _listener;
  bool _socket_waiting;
  e_step _step;
  char const* _step_string;
  std::queue<recv_data::pointer> _recv_queue;
  std::queue<send_data::pointer> _send_queue;
  bool _writing;

  using async_list = std::list<ssh2_action::pointer>;
  async_list _async_listeners;
  asio::system_timer _second_timer, _connect_timer;
};  // namespace sessions

template <class action_type, class callback_type>
void session::async_wait(action_type&& action,
                         callback_type&& callback,
                         const time_point& time_out,
                         const char* debug_info) {
  int retval = action();
  if (retval != LIBSSH2_ERROR_EAGAIN) {
    callback(retval);
    return;
  }
  _async_listeners.emplace_front(
      std::make_shared<ssh2_action>(action, callback, time_out, debug_info));
}

std::ostream& operator<<(std::ostream& os, const session& sess);

}  // namespace sessions

CCCS_END()

namespace fmt {
// formatter specializations for fmt
template <>
struct formatter<com::centreon::connector::ssh::sessions::session>
    : ostream_formatter {};
}  // namespace fmt

#endif  // !CCCS_SESSIONS_SESSION_HH
