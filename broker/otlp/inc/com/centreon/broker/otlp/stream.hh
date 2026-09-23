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

namespace com::centreon::broker::otlp {

/**
 * @brief Output stream exporting check results over OTLP/gRPC.
 *
 * Only the endpoint's processing::failover thread calls write/flush/stop, so
 * the stream sees a single writer; _protect guards against the gRPC
 * completion threads instead.
 */
class stream : public io::stream {
  const otlp_config::pointer _conf;
  std::shared_ptr<spdlog::logger> _logger;

 public:
  stream(const otlp_config::pointer& conf,
         const std::shared_ptr<spdlog::logger>& logger);
  ~stream() noexcept override = default;

  bool read(std::shared_ptr<io::data>& d, time_t deadline) override;
  int write(const std::shared_ptr<io::data>& d) override;
  int flush() override;
  int32_t stop() override;
  void statistics(nlohmann::json& tree) const override;
};

}  // namespace com::centreon::broker::otlp

#endif  // !CCB_OTLP_STREAM_HH
