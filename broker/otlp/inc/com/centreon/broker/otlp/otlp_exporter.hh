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

#ifndef CCB_OTLP_EXPORTER_HH
#define CCB_OTLP_EXPORTER_HH

#include "com/centreon/broker/otlp/otlp_config.hh"
#include "com/centreon/common/grpc/grpc_client.hh"
#include "opentelemetry/proto/collector/metrics/v1/metrics_service.grpc.pb.h"

namespace com::centreon::broker::otlp {

/**
 * @brief What the stream needs from an exporter.
 *
 * Separated from the gRPC implementation so stream tests do not have to stand
 * up a collector or a channel.
 */
class exporter_base {
 public:
  using ExportRequest = ::opentelemetry::proto::collector::metrics::v1::
      ExportMetricsServiceRequest;
  using ExportResponse = ::opentelemetry::proto::collector::metrics::v1::
      ExportMetricsServiceResponse;
  /* status, response, number of datapoints the request carried */
  using export_callback =
      std::function<void(const ::grpc::Status&, const ExportResponse&,
                         uint64_t)>;

  virtual ~exporter_base() = default;

  /**
   * @brief Send a batch. Returns without waiting for the collector.
   */
  virtual void export_async(ExportRequest&& request,
                            uint64_t nb_data,
                            export_callback cb) = 0;
};

/**
 * @brief OTLP/gRPC metrics exporter.
 *
 * MetricsService::Export is a unary RPC, so this uses the callback API rather
 * than the reactor machinery the engine's bidirectional agent client needs.
 * The channel, TLS material, keepalive and token injection all come from
 * grpc_client_base.
 */
class otlp_exporter : public exporter_base,
                      public com::centreon::common::grpc::grpc_client_base {
 private:
  /* Request, response and context must outlive the async call; they are held
   * here and kept alive by the completion lambda. */
  struct pending_call {
    ::grpc::ClientContext ctx;
    ExportRequest request;
    ExportResponse response;
    uint64_t nb_data;
  };

  std::unique_ptr<
      ::opentelemetry::proto::collector::metrics::v1::MetricsService::Stub>
      _stub;
  const otlp_config::pointer _conf;

 public:
  otlp_exporter(const otlp_config::pointer& conf,
                const std::shared_ptr<spdlog::logger>& logger);

  /**
   * @brief Send a batch. Returns immediately; cb runs on a gRPC thread.
   */
  void export_async(ExportRequest&& request,
                    uint64_t nb_data,
                    export_callback cb) override;
};

}  // namespace com::centreon::broker::otlp

#endif  // !CCB_OTLP_EXPORTER_HH
