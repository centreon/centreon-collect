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

#ifndef CCB_OTLP_STREAM_HH
#define CCB_OTLP_STREAM_HH

#include "com/centreon/broker/io/stream.hh"
#include "com/centreon/broker/otlp/otlp_config.hh"
#include "com/centreon/broker/otlp/otlp_exporter.hh"
#include "com/centreon/broker/otlp/request_builder.hh"
#include "com/centreon/broker/otlp/resource_enricher.hh"

namespace com::centreon::broker::otlp {

/**
 * @brief Output stream exporting Centreon check results over OTLP/gRPC.
 *
 * Only the endpoint's processing::failover thread calls write/flush/stop, so
 * the stream sees a single writer; _protect guards against the gRPC
 * completion threads instead.
 *
 * No queue is kept beyond the batch being assembled: unacknowledged events
 * stay in the muxer and spill to the retention file, which is broker core's
 * job rather than the module's.
 */
class stream : public io::stream {
  const otlp_config::pointer _conf;
  std::shared_ptr<spdlog::logger> _logger;
  std::shared_ptr<resource_enricher> _enricher;
  std::shared_ptr<exporter_base> _exporter;

  mutable std::mutex _protect;
  std::unique_ptr<request_builder> _builder;
  /* Events delivered but not yet reported to the muxer. */
  uint32_t _acknowledged = 0;
  uint32_t _inflight = 0;
  std::time_t _last_send = 0;

  /* Statistics. */
  uint64_t _stat_batches_sent = 0;
  uint64_t _stat_datapoints_sent = 0;
  uint64_t _stat_export_errors = 0;
  uint64_t _stat_dropped_no_host_name = 0;

  /* A batch detached from the builder and ready to hand to the exporter. */
  struct pending_export {
    exporter_base::ExportRequest request;
    uint64_t nb_data;
  };

  /**
   * @brief Detach the current batch if it should go out now.
   *
   * Call with _protect held. The returned batch must be dispatched *after*
   * releasing the lock: the exporter may invoke its completion callback
   * synchronously, and that callback takes _protect too.
   */
  std::optional<pending_export> _prepare_send_locked();
  /** Hand a detached batch to the exporter. Call without _protect held. */
  void _dispatch(pending_export&& batch);
  int _take_acknowledged_locked();

 public:
  stream(const otlp_config::pointer& conf,
         const std::shared_ptr<resource_enricher>& enricher,
         const std::shared_ptr<exporter_base>& exporter,
         const std::shared_ptr<spdlog::logger>& logger);
  ~stream() noexcept override = default;

  bool read(std::shared_ptr<io::data>& d, time_t deadline) override;
  int write(std::shared_ptr<io::data> const& d) override;
  int flush() override;
  int32_t stop() override;
  void statistics(nlohmann::json& tree) const override;
};

}  // namespace com::centreon::broker::otlp

#endif  // !CCB_OTLP_STREAM_HH
