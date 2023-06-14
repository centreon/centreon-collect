/*
 * Copyright 2011-2012, 2021, 2023 Centreon
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

#ifndef CCB_IO_ENDPOINT_HH
#define CCB_IO_ENDPOINT_HH

#include <nlohmann/json.hpp>

#include "com/centreon/broker/io/stream.hh"
#include "com/centreon/broker/multiplexing/muxer_filter.hh"
#include "com/centreon/broker/namespace.hh"

CCB_BEGIN()

// Forward declaration.
class persistent_cache;

namespace io {
/**
 *  @class endpoint endpoint.hh "com/centreon/broker/io/endpoint.hh"
 *  @brief Base class of connectors and acceptors.
 *
 *  Endpoint are used to open data streams. Endpoints can be either
 *  acceptors (which wait for incoming connections) or connectors
 *  (that initiate connections).
 */
class endpoint {
  bool _is_acceptor;
  /* The mandatory filters for the stream configured from this endpoint to
   * correctly work. This object can be empty. It filters categories and
   * elements. */
  const multiplexing::muxer_filter _stream_mandatory_filter;

 protected:
  std::shared_ptr<endpoint> _from;

 public:
  endpoint(bool is_accptr, const multiplexing::muxer_filter& filter);
  endpoint(const endpoint& other);
  virtual ~endpoint() noexcept = default;
  endpoint& operator=(const endpoint& other) = delete;
  void from(std::shared_ptr<endpoint> endp);
  bool is_acceptor() const noexcept;
  bool is_connector() const noexcept;
  virtual std::shared_ptr<stream> open() = 0;
  virtual bool is_ready() const;
  virtual void stats(nlohmann::json& tree);

  /**
   * @brief accessor to the filter wanted by the stream used by this endpoint.
   * This filter is defined by the stream itself and cannot change.
   *
   * @return A multiplexing::muxer_filter reference.
   */
  const multiplexing::muxer_filter& get_stream_mandatory_filter() const {
    return _stream_mandatory_filter;
  }
};
}  // namespace io

CCB_END()

#endif  // !CCB_IO_ENDPOINT_HH
