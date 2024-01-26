/**
 * Copyright 2009-2013, 2021 Centreon
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

#include "com/centreon/broker/tls/acceptor.hh"

#include <gnutls/gnutls.h>

#include "com/centreon/broker/log_v2.hh"
#include "com/centreon/broker/tls/stream.hh"
#include "com/centreon/exceptions/msg_fmt.hh"

using namespace com::centreon::broker;
using namespace com::centreon::exceptions;
using namespace com::centreon::broker::tls;

/**************************************
 *                                     *
 *           Public Methods            *
 *                                     *
 **************************************/

/**
 *  Default constructor.
 *
 *  @param[in] cert Certificate.
 *  @param[in] key  Key file.
 *  @param[in] ca   Trusted CA's certificate.
 */
acceptor::acceptor(std::string const& cert,
                   std::string const& key,
                   std::string const& ca,
                   std::string const& tls_hostname)
    : io::endpoint(true, {}),
      _ca(ca),
      _cert(cert),
      _key(key),
      _tls_hostname(tls_hostname) {}

/**
 *  @brief Try to accept a new connection.
 *
 *  Wait for an incoming client through the underlying acceptor, perform
 *  TLS checks (if configured to do so) and return a TLS encrypted
 *  stream.
 *
 *  @return A TLS-encrypted stream (namely a tls::stream object).
 *
 *  @see tls::stream
 */
std::shared_ptr<io::stream> acceptor::open() {
  /*
  ** The process of accepting a TLS client is pretty straight-forward.
  ** Just follow the comments the have an overview of performed
  ** operations.
  */

  // First accept a client from the lower layer.
  std::shared_ptr<io::stream> lower(_from->open());
  if (lower)
    return open(std::move(lower));
  return nullptr;
}

/**
 *  Overload of open, using base stream.
 *
 *  @param[in] lower Open stream.
 *
 *  @return Encrypted stream.
 */
std::shared_ptr<io::stream> acceptor::open(
    const std::shared_ptr<io::stream>& lower) {
  std::shared_ptr<stream> u;
  if (lower) {
    int ret;

    // Load parameters.
    params p(params::SERVER);
    p.set_cert(_cert, _key);
    p.set_trusted_ca(_ca);
    p.set_tls_hostname(_tls_hostname);
    p.load();

    // Create stream object.
    u = std::make_shared<stream>(GNUTLS_SERVER | GNUTLS_NONBLOCK);
    u->set_substream(lower);
    u->init(p);
  }

  return u;
}
