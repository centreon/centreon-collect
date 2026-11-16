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

#ifndef CCB_VICTORIA_METRICS_SERIES_CONFIG_HH
#define CCB_VICTORIA_METRICS_SERIES_CONFIG_HH

#include <absl/base/thread_annotations.h>
#include <absl/container/flat_hash_map.h>
#include <absl/synchronization/mutex.h>

#include <memory>
#include <optional>
#include <string>
#include <string_view>

namespace com::centreon::broker::victoria_metrics {

/**
 * @brief name and unit of the metrics seen by the output. Rebuild messages
 * carry neither, and the replayed points must land in the series of the live
 * points. Shared by the successive streams of one endpoint.
 */
class metric_labels {
 public:
  struct labels {
    std::string name;
    std::string unit;
  };

  void set(uint64_t metric_id, const std::string& name, const std::string& unit)
      ABSL_LOCKS_EXCLUDED(_protect);
  std::optional<labels> get(uint64_t metric_id) const
      ABSL_LOCKS_EXCLUDED(_protect);
  void erase(uint64_t metric_id) ABSL_LOCKS_EXCLUDED(_protect);

 private:
  mutable absl::Mutex _protect;
  absl::flat_hash_map<uint64_t, labels> _labels ABSL_GUARDED_BY(_protect);
};

/**
 * @brief VictoriaMetrics settings that http_tsdb does not know.
 */
struct series_config {
  static constexpr std::string_view default_delete_target =
      "/api/v1/admin/tsdb/delete_series";
  static constexpr std::string_view default_series_target = "/api/v1/series";

  // endpoint key http_delete_target
  std::string delete_target{default_delete_target};
  // endpoint key http_series_target
  std::string series_target{default_series_target};
  std::shared_ptr<metric_labels> labels = std::make_shared<metric_labels>();
};

}  // namespace com::centreon::broker::victoria_metrics

#endif  // !CCB_VICTORIA_METRICS_SERIES_CONFIG_HH
