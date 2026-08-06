/**
 * Copyright 2026 Centreon
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

#ifndef CCB_OTLP_CONFIG_HH
#define CCB_OTLP_CONFIG_HH

#include "com/centreon/common/grpc/grpc_config.hh"

namespace com::centreon::broker::otlp {

/**
 * @brief Endpoint configuration of the OTLP output.
 *
 * The mapping from Centreon metrics to semantic conventions is deliberately
 * not configurable; only whether the optional annotation streams are emitted.
 */
struct otlp_config {
  using pointer = std::shared_ptr<otlp_config>;

  /* gRPC channel: host:port, TLS material, token, compression. Reused
   * verbatim by otlp_exporter through common::grpc::grpc_client_base. */
  com::centreon::common::grpc::grpc_config::pointer grpc;

  /* Datapoints buffered before an export is triggered. */
  uint32_t max_datapoints_per_batch = 5000;
  /* Seconds after which a partial batch is flushed anyway. */
  uint32_t max_send_interval = 10;
  /* Concurrent in-flight Export calls. Bounds memory when the collector is
   * slow; retention itself belongs to the muxer. */
  uint32_t max_inflight_requests = 4;
  /* Per-export deadline, in seconds. */
  uint32_t export_timeout = 30;

  /* Optional annotation streams. On by default. */
  bool send_thresholds = true;
  bool send_status = true;
  bool send_min_max = true;
};

}  // namespace com::centreon::broker::otlp

#endif  // !CCB_OTLP_CONFIG_HH
