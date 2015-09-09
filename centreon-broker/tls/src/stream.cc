/*
** Copyright 2009-2014 Centreon
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

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include "com/centreon/broker/exceptions/msg.hh"
#include "com/centreon/broker/io/events.hh"
#include "com/centreon/broker/io/exceptions/shutdown.hh"
#include "com/centreon/broker/io/raw.hh"
#include "com/centreon/broker/logging/logging.hh"
#include "com/centreon/broker/tls/stream.hh"

using namespace com::centreon::broker;
using namespace com::centreon::broker::tls;

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
stream::stream(gnutls_session_t* sess)
  : _deadline((time_t)-1), _session(sess) {}

/**
 *  @brief Destructor.
 *
 *  The destructor will release all acquired ressources that haven't
 *  been released yet.
 */
stream::~stream() {
  if (_session) {
    _deadline = time(NULL) + 30; // XXX : use connection timeout
    gnutls_bye(*_session, GNUTLS_SHUT_RDWR);
    gnutls_deinit(*_session);
    delete (_session);
    _session = NULL;
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
bool stream::read(misc::shared_ptr<io::data>& d, time_t deadline) {
  // Clear existing content.
  d.clear();

  // Read data.
  _deadline = deadline;
  misc::shared_ptr<io::raw> buffer(new io::raw);
  buffer->resize(BUFSIZ);
  int ret(gnutls_record_recv(
            *_session,
            buffer->QByteArray::data(),
            buffer->size()));
  if (ret < 0) {
    if ((ret != GNUTLS_E_INTERRUPTED) && (ret != GNUTLS_E_AGAIN))
      throw (exceptions::msg() << "TLS: could not receive data: "
             << gnutls_strerror(ret));
    else
      return (false);
  }
  else if (ret) {
    buffer->resize(ret);
    d = buffer;
    return (true);
  }
  else
    throw (exceptions::msg() << "TLS session is terminated");
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
  // Read some data.
  bool timed_out(false);
  while (_buffer.isEmpty()) {
    misc::shared_ptr<io::data> d;
    timed_out = !_substream->read(d, _deadline);
    if (!d.isNull() && (d->type() == io::raw::static_type())) {
      io::raw* r(static_cast<io::raw*>(d.data()));
      _buffer.append(r->QByteArray::data(), r->size());
    }
    else if (timed_out)
      break ;
  }

  // Transfer data.
  unsigned int rb(_buffer.size());
  if (!rb) {
    if (timed_out) {
      gnutls_transport_set_errno(*_session, EAGAIN);
      return (-1);
    }
    else {
      return (0);
    }
  }
  else if (size >= rb) {
    memcpy(buffer, _buffer.data(), rb);
    _buffer.clear();
    return (rb);
  }
  else {
    memcpy(buffer, _buffer.data(), size);
    _buffer.remove(0, size);
    return (size);
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
unsigned int stream::write(misc::shared_ptr<io::data> const& d) {
  // Send data.
  if (!d.isNull() && d->type() == io::raw::static_type()) {
    io::raw const* packet(static_cast<io::raw const*>(d.data()));
    char const* ptr(packet->QByteArray::data());
    int size(packet->size());
    while (size > 0) {
      int ret(gnutls_record_send(
                *_session,
                ptr,
                size));
      if (ret < 0)
        throw (exceptions::msg() << "TLS: could not send data: "
               << gnutls_strerror(ret));
      ptr += ret;
      size -= ret;
    }
  }

  return (1);
}

/**
 *  Write encrypted data to base stream.
 *
 *  @param[in] buffer Data to write.
 *  @param[in] size   Size of buffer.
 *
 *  @return Number of bytes written.
 */
long long stream::write_encrypted(
                    void const* buffer,
                    long long size) {
  misc::shared_ptr<io::raw> r(new io::raw);
  r->append(static_cast<char const*>(buffer), size);
  _substream->write(r);
  _substream->write(misc::shared_ptr<io::data>());
  return (size);
}
