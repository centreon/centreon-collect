/**
 * Copyright 2022-2024 Centreon
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

#ifndef CCB_IO_LIMIT_ENDPOINT_HH
#define CCB_IO_LIMIT_ENDPOINT_HH

#include "endpoint.hh"

namespace com::centreon::broker::io {

class limit_endpoint : public endpoint {
 protected:
  /* How many consecutive calls to is_ready() */
  mutable int16_t _is_ready_count;
  /* The time of the last call to is_ready() */
  mutable std::time_t _is_ready_now;

 public:
  /**
   * @brief Constructor of a limit_endpoint.
   *
   * @param is_accptr A boolean to tell if this endpoint is an acceptor.
   * @param mandatory_filter Mandatory filters for the stream.
   * @param forbidden_filter Forbidden filters for the stream.
   */
  limit_endpoint(bool is_accptr,
                 const multiplexing::muxer_filter& mandatory_filter,
                 const multiplexing::muxer_filter& forbidden_filter)
      : endpoint(is_accptr, mandatory_filter, forbidden_filter),
        _is_ready_count(0),
        _is_ready_now(0) {}

  std::shared_ptr<stream> open() override;
  bool is_ready() const override;

  virtual std::shared_ptr<stream> create_stream() = 0;
};

}  // namespace com::centreon::broker::io

#endif  // !CCB_IO_LIMIT_ENDPOINT_HH
