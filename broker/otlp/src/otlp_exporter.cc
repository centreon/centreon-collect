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

#include "com/centreon/broker/otlp/otlp_exporter.hh"

using namespace com::centreon::broker::otlp;

otlp_exporter::otlp_exporter(const otlp_config::pointer& conf,
                             const std::shared_ptr<spdlog::logger>& logger)
    : com::centreon::common::grpc::grpc_client_base(conf->grpc, logger),
      _stub(::opentelemetry::proto::collector::metrics::v1::MetricsService::
                NewStub(_channel)),
      _conf(conf) {}

void otlp_exporter::export_async(ExportRequest&& request,
                                 uint64_t nb_data,
                                 export_callback cb) {
  auto call = std::make_shared<pending_call>();
  call->request = std::move(request);
  call->nb_data = nb_data;
  call->ctx.set_deadline(std::chrono::system_clock::now() +
                         std::chrono::seconds(_conf->export_timeout));

  if (_conf->grpc->is_compressed()) {
    /* Override the channel default, which grpc_client_base derives from
     * grpc_compression_algorithm_for_level() and which resolves to deflate.
     * Deflate is fine between two Centreon peers, both being C++ gRPC, but the
     * OpenTelemetry Collector is written in Go and Go's gRPC only registers a
     * gzip compressor, so it answers "Decompressor is not installed for
     * grpc-encoding deflate" and every export fails. gzip is the one algorithm
     * every OTLP implementation supports. */
    call->ctx.set_compression_algorithm(GRPC_COMPRESS_GZIP);
  }

  auto logger = get_logger();
  _stub->async()->Export(
      &call->ctx, &call->request, &call->response,
      [call, cb = std::move(cb), logger](const ::grpc::Status& status) {
        if (!status.ok()) {
          SPDLOG_LOGGER_ERROR(logger, "otlp: export of {} datapoints failed: {}",
                              call->nb_data, status.error_message());
        } else if (call->response.has_partial_success() &&
                   call->response.partial_success().rejected_data_points() >
                       0) {
          /* Already acknowledged upstream, so this cannot be retried; it is
           * reported so a misconfigured collector is visible. */
          SPDLOG_LOGGER_ERROR(
              logger, "otlp: collector rejected {} datapoints: {}",
              call->response.partial_success().rejected_data_points(),
              call->response.partial_success().error_message());
        }
        cb(status, call->response, call->nb_data);
      });
}
