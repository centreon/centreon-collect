/**
* Copyright 2011-2013 Centreon
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
#include "com/centreon/broker/io/endpoint.hh"

#include "com/centreon/broker/persistent_cache.hh"

using namespace com::centreon::broker::io;

/**
 *  Default constructor.
 *
 *  @param[in] is_accptr  True if endpoint is an acceptor.
 */
endpoint::endpoint(bool is_accptr, const multiplexing::muxer_filter& filter)
    : _is_acceptor(is_accptr), _stream_mandatory_filter{filter} {}

/**
 *  Copy constructor.
 *
 *  @param[in] other  Object to copy.
 */
endpoint::endpoint(endpoint const& other)
    : _is_acceptor(other._is_acceptor),
      _stream_mandatory_filter{other._stream_mandatory_filter},
      _from(other._from) {}

/**
 *  Set the lower layer endpoint object of this endpoint.
 *
 *  @param[in] endp Lower layer endpoint object.
 */
void endpoint::from(std::shared_ptr<endpoint> endp) {
  _from = endp;
}

/**
 *  Check if this endpoint is an acceptor.
 *
 *  @return true if endpoint is an acceptor.
 */
bool endpoint::is_acceptor() const noexcept {
  return _is_acceptor;
}

/**
 *  Check if this endpoint is a connector.
 *
 *  @return true if endpoint is a connector.
 */
bool endpoint::is_connector() const noexcept {
  return !_is_acceptor;
}

/**
 *  Generate statistics about the endpoint.
 *
 *  @param[out] tree Properties tree.
 */
void endpoint::stats(nlohmann::json& tree) {
  if (_from)
    _from->stats(tree);
}

/**
 * @brief Return True if the call to open() should work.
 *
 * @return A boolean.
 */
bool endpoint::is_ready() const {
  if (_from)
    return _from->is_ready();
  return false;
}
