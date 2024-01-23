/**
 * Copyright 2009-2013 Centreon
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

#include "com/centreon/broker/tls/connector.hh"

#include "com/centreon/broker/log_v2.hh"
#include "com/centreon/broker/tls/stream.hh"
#include "com/centreon/exceptions/msg_fmt.hh"

using namespace com::centreon::broker;
using namespace com::centreon::broker::tls;
using namespace com::centreon::exceptions;

/**************************************
 *                                     *
 *           Public Methods            *
 *                                     *
 **************************************/

/**
 *  Default constructor
 *
 *  @param[in] cert Certificate.
 *  @param[in] key  Key file.
 *  @param[in] ca   Trusted CA's certificate.
 */
connector::connector(std::string const& cert,
                     std::string const& key,
                     std::string const& ca,
                     std::string const& tls_hostname)
    : io::endpoint(false, {}),
      _ca(ca),
      _cert(cert),
      _key(key),
      _tls_hostname(tls_hostname) {}

/**
 *  Connect to the remote TLS peer.
 *
 *  @return New connected stream.
 */
std::shared_ptr<io::stream> connector::open() {
  // First connect the lower layer.
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
std::shared_ptr<io::stream> connector::open(std::shared_ptr<io::stream> lower) {
  std::shared_ptr<stream> u;
  if (lower) {
    int ret;
    // Load parameters.
    params p(params::CLIENT);
    p.set_cert(_cert, _key);
    p.set_trusted_ca(_ca);
    p.set_tls_hostname(_tls_hostname);
    p.load();

    std::unique_ptr<gnutls_session_t> session =
        std::make_unique<gnutls_session_t>();
    // Initialize the TLS session
    SPDLOG_LOGGER_DEBUG(log_v2::tls(), "TLS: initializing session");
#ifdef GNUTLS_NONBLOCK
    ret = gnutls_init(session.get(), GNUTLS_CLIENT | GNUTLS_NONBLOCK);
#else
    ret = gnutls_init(session, GNUTLS_CLIENT);
#endif  // GNUTLS_NONBLOCK
    if (ret != GNUTLS_E_SUCCESS) {
      SPDLOG_LOGGER_ERROR(log_v2::tls(), "TLS: cannot initialize session: {}",
                          gnutls_strerror(ret));
      throw msg_fmt("TLS: cannot initialize session: {} ",
                    gnutls_strerror(ret));
    }

    // Apply TLS parameters to the current session.
    p.apply(*session);

    // Create stream object.
    u = std::make_shared<stream>(std::move(session));
    u->set_substream(lower);
    u->init(p);
  }

  return u;
}
