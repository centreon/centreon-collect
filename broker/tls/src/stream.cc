/**
 * Copyright 2009-2017 Centreon
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

#include <cerrno>

#include "com/centreon/broker/io/events.hh"
#include "com/centreon/broker/io/raw.hh"
#include "com/centreon/broker/log_v2.hh"
#include "com/centreon/exceptions/msg_fmt.hh"

#include "com/centreon/broker/tls/internal.hh"
#include "com/centreon/broker/tls/stream.hh"

using namespace com::centreon::broker;
using namespace com::centreon::broker::tls;
using namespace com::centreon::exceptions;

/**************************************
 *                                     *
 *           Public Methods            *
 *                                     *
 **************************************/

/**
 *  @brief Constructor.
 *
 *  When building the stream, you need to provide the session that will
 *  be used to transport encrypted data.
 *
 *  @param[in] sess  TLS session, providing informations on the
 *                   encryption that should be used.
 */
stream::stream(unsigned int session_flags)
    : io::stream("TLS"), _deadline((time_t)-1), _session(nullptr) {
  SPDLOG_LOGGER_DEBUG(log_v2::tls(), "{:p} TLS: created",
                      static_cast<const void*>(this));
  int ret = gnutls_init(&_session, session_flags);
  if (ret != GNUTLS_E_SUCCESS) {
    SPDLOG_LOGGER_ERROR(log_v2::tls(), "TLS: cannot initialize session: {}",
                        gnutls_strerror(ret));
    throw msg_fmt("TLS: cannot initialize session: {}", gnutls_strerror(ret));
  }
}

/**
 * @brief this mehtod initialize crypto of the stream
 *
 * @param param crypto params that will validate cert of the session
 */
void stream::init(const params& param) {
  int ret;
  param.apply(_session);

  // Bind the TLS session with the stream from the lower layer.
#if GNUTLS_VERSION_NUMBER < 0x020C00
  gnutls_transport_set_lowat(*session, 0);
#endif  // GNU TLS < 2.12.0
  gnutls_transport_set_pull_function(_session, pull_helper);
  gnutls_transport_set_push_function(_session, push_helper);
  gnutls_transport_set_ptr(_session, this);

  // Perform the TLS handshake.
  SPDLOG_LOGGER_DEBUG(log_v2::tls(), "{:p} TLS: performing handshake",
                      static_cast<const void*>(this));
  do {
    ret = gnutls_handshake(_session);
  } while (GNUTLS_E_AGAIN == ret || GNUTLS_E_INTERRUPTED == ret);
  if (ret != GNUTLS_E_SUCCESS) {
    SPDLOG_LOGGER_ERROR(log_v2::tls(), "TLS: handshake failed: {}",
                        gnutls_strerror(ret));
    throw msg_fmt("TLS: handshake failed: {} ", gnutls_strerror(ret));
  }
  SPDLOG_LOGGER_DEBUG(log_v2::tls(), "TLS: successful handshake");
  gnutls_protocol_t prot = gnutls_protocol_get_version(_session);
  gnutls_cipher_algorithm_t ciph = gnutls_cipher_get(_session);
  SPDLOG_LOGGER_DEBUG(log_v2::tls(), "TLS: protocol and cipher  {} {} used",
                      gnutls_protocol_get_name(prot),
                      gnutls_cipher_get_name(ciph));

  // Check certificate.
  param.validate_cert(_session);
}

/**
 *  @brief Destructor.
 *
 *  The destructor will release all acquired ressources that haven't
 *  been released yet.
 */
stream::~stream() {
  if (_session) {
    try {
      SPDLOG_LOGGER_DEBUG(log_v2::tls(), "{:p} TLS: destroy session: {:p}",
                          static_cast<const void*>(this),
                          static_cast<const void*>(_session));
      gnutls_bye(_session, GNUTLS_SHUT_RDWR);
      gnutls_deinit(_session);
    }
    // Ignore exception whatever the error might be.
    catch (...) {
    }
  }
}

/**
 *  @brief Receive data from the TLS session.
 *
 *  Receive at most size bytes from the network stream and store them in
 *  buffer. The number of bytes read is then returned. This number can
 *  be less than size.
 *
 *  @param[out] d         Object that will be returned containing a
 *                        chunk of data.
 *  @param[in]  deadline  Timeout.
 *
 *  @return Respect io::stream::read()'s return value.
 */
bool stream::read(std::shared_ptr<io::data>& d, time_t deadline) {
  // Clear existing content.
  d.reset();

  // Read data.
  _deadline = deadline;
  std::shared_ptr<io::raw> buffer(new io::raw);
  buffer->resize(BUFSIZ);
  int ret(gnutls_record_recv(_session, buffer->data(), buffer->size()));
  if (ret < 0) {
    if ((ret != GNUTLS_E_INTERRUPTED) && (ret != GNUTLS_E_AGAIN)) {
      SPDLOG_LOGGER_ERROR(log_v2::tls(), "TLS: could not receive data: {}",
                          gnutls_strerror(ret));
      throw msg_fmt("TLS: could not receive data: {} ", gnutls_strerror(ret));
    } else
      return false;
  } else if (ret) {
    buffer->resize(ret);
    d = buffer;
    return true;
  } else {
    SPDLOG_LOGGER_ERROR(log_v2::tls(), "TLS session is terminated");
    throw msg_fmt("TLS session is terminated");
  }
  return false;
}

/**
 *  Read encrypted data from base stream.
 *
 *  @param[out] buffer Output buffer.
 *  @param[in]  size   Maximum size.
 *
 *  @return Number of bytes actually read.
 */
long long stream::read_encrypted(void* buffer, long long size) {
  try {
    // Read some data.
    bool timed_out(false);
    while (_buffer.empty()) {
      std::shared_ptr<io::data> d;
      timed_out = !_substream->read(d, _deadline);
      if (!timed_out && d && d->type() == io::raw::static_type()) {
        io::raw* r(static_cast<io::raw*>(d.get()));
        _buffer.reserve(_buffer.size() + r->get_buffer().size());
        _buffer.insert(_buffer.end(), r->get_buffer().begin(),
                       r->get_buffer().end());
        //_buffer.append(r->data(), r->size());
      } else if (timed_out)
        break;
    }

    // Transfer data.
    uint32_t rb(_buffer.size());
    if (!rb) {
      if (timed_out) {
        gnutls_transport_set_errno(_session, EAGAIN);
        return -1;
      } else {
        return 0;
      }
    } else if (size >= rb) {
      memcpy(buffer, _buffer.data(), rb);
      _buffer.clear();
      return rb;
    } else {
      memcpy(buffer, _buffer.data(), size);
      _buffer.erase(_buffer.begin(), _buffer.begin() + size);
      //_buffer.remove(0, size);
      return size;
    }
  } catch (const std::exception& e) {
    SPDLOG_LOGGER_DEBUG(log_v2::tls(), "tls read fail: {}", e.what());
    gnutls_transport_set_errno(_session, EPIPE);
    throw;
  }
}

/**
 *  @brief Send data across the TLS session.
 *
 *  Send a chunk of data.
 *
 *  @param[in] d Packet to send.
 *
 *  @return Number of events acknowledged.
 */
int stream::write(std::shared_ptr<io::data> const& d) {
  if (!validate(d, get_name()))
    return 1;

  // Send data.
  if (d->type() == io::raw::static_type()) {
    io::raw const* packet(static_cast<io::raw const*>(d.get()));
    char const* ptr(packet->const_data());
    int size(packet->size());
    while (size > 0) {
      int ret(gnutls_record_send(_session, ptr, size));
      if (ret < 0) {
        SPDLOG_LOGGER_ERROR(log_v2::tls(), "TLS: could not send data: {}",
                            gnutls_strerror(ret));
        throw msg_fmt("TLS: could not send data: {}", gnutls_strerror(ret));
      }
      ptr += ret;
      size -= ret;
    }
  }

  return 1;
}

/**
 *  Write encrypted data to base stream.
 *
 *  @param[in] buffer Data to write.
 *  @param[in] size   Size of buffer.
 *
 *  @return Number of bytes written.
 */
long long stream::write_encrypted(void const* buffer, long long size) {
  std::shared_ptr<io::raw> r(new io::raw);
  std::vector<char> tmp(const_cast<char*>(static_cast<char const*>(buffer)),
                        const_cast<char*>(static_cast<char const*>(buffer)) +
                            static_cast<std::size_t>(size));
  SPDLOG_LOGGER_TRACE(log_v2::tls(), "tls write enc: {}", size);
  r->get_buffer() = std::move(tmp);
  try {
    _substream->write(r);
    _substream->flush();
  } catch (const std::exception& e) {
    SPDLOG_LOGGER_DEBUG(log_v2::tls(), "tls write fail: {}", e.what());
    gnutls_transport_set_errno(_session, EPIPE);
    throw;
  }
  return size;
}
