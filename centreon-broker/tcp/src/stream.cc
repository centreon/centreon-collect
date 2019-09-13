/*
 * Copyright 2011 - 2019 Centreon (https://www.centreon.com/)
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * For more information : contact@centreon.com
 *
 */

#include "com/centreon/broker/tcp/stream.hh"
#include <sys/socket.h>
#include <sys/time.h>
#include <atomic>
#include <functional>
#include <sstream>
#include <system_error>
#include "com/centreon/broker/exceptions/msg.hh"
#include "com/centreon/broker/io/raw.hh"
#include "com/centreon/broker/logging/logging.hh"
#include "com/centreon/broker/tcp/acceptor.hh"

using namespace com::centreon::broker;
using namespace com::centreon::broker::tcp;

/**************************************
 *                                     *
 *           Public Methods            *
 *                                     *
 **************************************/

/**
 *  Constructor.
 *
 *  @param[in] sock  Socket used by this stream.
 *  @param[in] name  Name of this connection.
 */
stream::stream(asio::io_context& ctx,
               asio::ip::tcp::socket* sock,
               std::string const& name)
    : _name(name),
      _parent(nullptr),
      _read_timeout(-1),
      _socket(sock),
      _write_timeout(-1),
      _io_context{ctx} {
  _set_socket_options();
}

/**
 *  Destructor.
 */
stream::~stream() {
  try {
    // Close the socket.
    if (_socket)
      _socket->close();
    // Remove from parent.
    if (_parent)
      _parent->remove_child(_name);
  }
  // Ignore exception, whatever the error might be.
  catch (...) {
  }
}

/**
 *  Get peer name.
 *
 *  @return Peer name.
 */
std::string stream::peer() const {
  std::ostringstream oss;
  oss << "tcp://" << _socket->remote_endpoint().address().to_string() << ":"
      << _socket->remote_endpoint().port();
  return oss.str();
}

enum reason { reason_no, reason_timer, reason_socket };

static void timer_cb(std::atomic_uint8_t *reason,
                     std::error_code* return_error,
                     std::error_code err) {
  *reason = reason_timer;
  *return_error = err;
}

static void read_cb(std::atomic_uint8_t *reason,
                    asio::system_timer* tmr,
                    std::error_code* return_error,
                    std::size_t* return_len,
                    std::error_code err,
                    std::size_t bytes_transferred) {
  *reason = reason_socket;
  tmr->cancel();
  *return_error = err;
  *return_len = bytes_transferred;
}

/**
 *  Read data with timeout.
 *
 *  @param[out] d         Received event if any.
 *  @param[in]  deadline  Timeout in seconds.
 *
 *  @return Respects io::stream::read()'s return value.
 */

bool stream::read(std::shared_ptr<io::data>& d, time_t deadline) {
  // Check that socket exist.
  if (!_socket)
    _initialize_socket();

  std::atomic_uint8_t reason{reason_no};
  // Set deadline.
  {
    time_t now = ::time(nullptr);
    if (_read_timeout != -1 &&
        (deadline == (time_t)-1 || now + _read_timeout < deadline))
      deadline = now + _read_timeout;
  }

  asio::system_timer timer{_io_context};
  if (deadline != -1)
    timer.expires_at(std::chrono::system_clock::from_time_t(deadline));

  d.reset(new io::raw());

  std::error_code read_err;
  std::error_code timer_err;
  std::shared_ptr<io::raw> data{std::static_pointer_cast<io::raw>(d)};

  std::vector<char>& buffer{data->get_buffer()};
  buffer.resize(2048);
  std::size_t len{0};
  _socket->async_read_some(
      asio::buffer(buffer, 2048),
      std::bind(read_cb, &reason, &timer, &read_err, &len, std::placeholders::_1,
                std::placeholders::_2));
  if (deadline != -1)
    timer.async_wait(
      std::bind(timer_cb, &reason, &timer_err, std::placeholders::_1));

  _io_context.reset();
  while (_io_context.run_one()) {
    if (reason == reason_timer && !timer_err)
      break;
    if (reason == reason_socket && !read_err)
      break;
    if (read_err)
      timer.cancel();
    if (timer_err)
      if (timer_err != std::errc::operation_canceled)
        timer_err.clear();
      else
        _socket->cancel();
  }

  if (timer_err)
    throw exceptions::msg()
        << "TCP peer '" << _name << "' err: " << timer_err.message();
  if (read_err)
    throw exceptions::msg()
        << "TCP peer '" << _name << "' err: " << read_err.message();
  if (deadline != -1)
    timer.cancel();

  if (len) {
    buffer.resize(len);
    return true;
  }

  return false;
}

/**
 *  Set parent socket.
 *
 *  @param[in,out] parent  Parent socket.
 */
void stream::set_parent(acceptor* parent) {
  _parent = parent;
}

/**
 *  Set read timeout.
 *
 *  @param[in] secs  Timeout in seconds.
 */
void stream::set_read_timeout(int secs) {
  if (secs == -1)
    _read_timeout = -1;
  else
    _read_timeout = secs;
}

/**
 *  Set write timeout.
 *
 *  @param[in] secs  Write timeout in seconds.
 */
void stream::set_write_timeout(int secs) {
  if (secs == -1)
    _write_timeout = -1;
  else
    _write_timeout = secs;
}

/**
 *  Write data to the socket.
 *
 *  @param[in] d Data to write.
 *
 *  @return Number of events acknowledged.
 */
int stream::write(std::shared_ptr<io::data> const& d) {
  // Check that socket exist.
  if (!_socket)
    _initialize_socket();

  // Check that data exists and should be processed.
  if (!validate(d, "TCP"))
    return 1;

  if (d->type() == io::raw::static_type()) {
    std::shared_ptr<io::raw> r(std::static_pointer_cast<io::raw>(d));
    logging::debug(logging::low) << "TCP: write request of " << r->size()
                                 << " bytes to peer '" << _name << "'";

    std::error_code err;

    _socket->write_some(asio::buffer(r->data(), r->size()), err);

    if (err)
      throw exceptions::msg() << "TCP: error while writing to peer '" << _name
                              << "': " << err.message();
  }
  return 1;
}

/**************************************
 *                                     *
 *           Private Methods           *
 *                                     *
 **************************************/

/**
 *  Initialize socket if it was not already initialized.
 */
void stream::_initialize_socket() {
  _socket.reset(new asio::ip::tcp::socket{_io_context});
  {
    std::ostringstream oss;
    oss << _socket->remote_endpoint().address().to_string() << ":"
        << _socket->remote_endpoint().port();
    _name = oss.str();
  }
  if (_parent)
    _parent->add_child(_name);
  _set_socket_options();
}

/**
 *  Set various socket options.
 */
void stream::_set_socket_options() {
  // Set the SO_KEEPALIVE option.
  asio::socket_base::keep_alive option{true};
  _socket->set_option(option);

  // Set the write timeout option.
  if (_write_timeout >= 0) {
    struct timeval t;
    t.tv_sec = _write_timeout;
    t.tv_usec = 0;
    ::setsockopt(_socket->native_handle(), SOL_SOCKET, SO_SNDTIMEO, &t,
                 sizeof(t));
  }
}
