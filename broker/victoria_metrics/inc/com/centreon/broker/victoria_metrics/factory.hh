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

#ifndef CCB_VICTORIA_METRICS_FACTORY_HH
#define CCB_VICTORIA_METRICS_FACTORY_HH

#include "com/centreon/broker/http_tsdb/factory.hh"

namespace com::centreon::broker {

namespace victoria_metrics {
/**
 *  @class factory factory.hh "com/centreon/broker/victoria_metrics/factory.hh"
 *  @brief Victoria Metrics layer factory.
 *
 *  Build Victoria Metrics layer objects.
 */
class factory : public http_tsdb::factory {
 public:
  static const nlohmann::json default_extra_status_column;
  static const nlohmann::json default_extra_metric_column;

  factory();
  factory(factory const&) = delete;
  ~factory() = default;
  factory& operator=(factory const& other) = delete;
  io::endpoint* new_endpoint(
      config::endpoint& cfg,
      const std::map<std::string, std::string>& global_params,
      bool& is_acceptor,
      std::shared_ptr<persistent_cache> cache) const override;
};
}  // namespace victoria_metrics

}  // namespace com::centreon::broker

#endif  // !CCB_VICTORIA_METRICS_FACTORY_HH
