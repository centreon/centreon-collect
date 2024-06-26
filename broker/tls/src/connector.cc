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

#include "com/centreon/broker/multiplexing/muxer_filter.hh"
#include "com/centreon/broker/tls/stream.hh"
#include "com/centreon/exceptions/msg_fmt.hh"
#include "common/log_v2/log_v2.hh"

using namespace com::centreon::broker;
using namespace com::centreon::broker::tls;
using namespace com::centreon::exceptions;
using log_v2 = com::centreon::common::log_v2::log_v2;

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
    : io::endpoint(
          false,
          {},
          multiplexing::muxer_filter(multiplexing::muxer_filter::zero_init())),
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
    // Load parameters.
    params p(params::CLIENT);
    p.set_cert(_cert, _key);
    p.set_trusted_ca(_ca);
    p.set_tls_hostname(_tls_hostname);
    p.load();

    // Create stream object.
    u = std::make_shared<stream>(GNUTLS_CLIENT);
    u->set_substream(lower);
    u->init(p);
  }

  return u;
}
