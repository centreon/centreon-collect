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

#ifndef CCB_RRD_RETENTION_MANAGER_HH
#define CCB_RRD_RETENTION_MANAGER_HH

#include "com/centreon/broker/rrd/backend.hh"
#include "retention_file.hh"

namespace com::centreon::broker::rrd {

/**
 * @brief Configuration for the retention buffer.
 *
 * All fields are read from the RRD endpoint JSON configuration block.
 */
struct retention_config {
  std::filesystem::path metrics_dir;  ///< Directory of metric .rrd/.ret files
  std::filesystem::path status_dir;   ///< Directory of status .rrd/.ret files
  uint32_t orphan_interval = 3600;    ///< Max inactivity seconds before merge
  uint32_t merge_samples_threshold =
      60 * 24 / 5;  ///< when a retention file has more than
  ///< merge_sample_threshold samples => merge

  // if a retention_file had not been fed during interval *
  // proportional_delay_threshold, we merge
  uint32_t proportional_activity_threshold = 3;
};

/**
 * @class retention_manager
 * @brief Manages the retention files of the RRD metrics and statuses.
 *
 * A metric or status point older than the last update of its rrd file can't
 * be written by rrd_update, the stream gives it to write_metric() or
 * write_status() which append it to the retention file of the metric or status
 * (retention_file), created on the first point.
 *
 * On-disk layout (co-located with .rrd files):
 *   {metrics_dir}/{metric_id}.ret
 *   {status_dir}/{index_id}.ret
 * Existing files are reloaded by load().
 *
 * After each write, the stream calls start_merge() which, if no merge is
 * running, chooses a file to merge:
 *   - the least recently fed one if it has not been fed for orphan_interval
 *   - else the biggest one if it has more than merge_samples_threshold
 *     samples.
 * The merge runs on _io_context, then the retention file is removed and a
 * pb_end_metric_merge or pb_end_status_merge is published.
 */
class retention_manager
    : public std::enable_shared_from_this<retention_manager> {
  /// The directories never change, the thresholds are updated by
  /// _update_config() and read under _data_m.
  retention_config _config;
  std::shared_ptr<boost::asio::io_context> _io_context;
  std::shared_ptr<spdlog::logger> _logger;

  mutable absl::Mutex _data_m;

  struct retention_id_extractor {
    using result_type =
        std::pair<uint64_t /*index_id or metric_id*/, bool /*is_status*/>;
    result_type operator()(const std::shared_ptr<retention_file>& ret) const {
      return std::make_pair(ret->id(), ret->is_status());
    }
  };

  struct last_activity_extractor {
    using result_type = time_t;
    result_type operator()(const std::shared_ptr<retention_file>& ret) const {
      return ret->last_activity();
    }
  };

  struct nb_samples_extractor {
    using result_type = size_t;
    result_type operator()(const std::shared_ptr<retention_file>& ret) const {
      return ret->nb_samples();
    }
  };

  class proportional_activity_extractor {
    unsigned _threshold;

   public:
    using result_type = time_t;
    proportional_activity_extractor(unsigned theshold) : _threshold(theshold) {}

    time_t operator()(const std::shared_ptr<retention_file>& ret) const {
      return ret->last_activity() + ret->interval() * _threshold;
    }
  };

  using retention_cont = boost::multi_index::multi_index_container<
      std::shared_ptr<retention_file>,
      boost::multi_index::indexed_by<
          boost::multi_index::ordered_unique<retention_id_extractor>,
          boost::multi_index::ordered_non_unique<last_activity_extractor>,
          boost::multi_index::ordered_non_unique<nb_samples_extractor>,
          boost::multi_index::ordered_non_unique<
              proportional_activity_extractor>>>;

  retention_cont _retentions ABSL_GUARDED_BY(_data_m);
  std::shared_ptr<retention_file> _merging ABSL_GUARDED_BY(_data_m);

  using id_to_ts = absl::flat_hash_map<uint64_t, uint64_t>;
  id_to_ts _earliest_rt_metric ABSL_GUARDED_BY(_data_m);
  id_to_ts _earliest_rt_status ABSL_GUARDED_BY(_data_m);

  void _init();

  void _update_config(const retention_config& config);

  void _merge(const std::shared_ptr<retention_file>& to_merge,
              const std::shared_ptr<lib>& rrd_lib,
              backend* cached_client) ABSL_EXCLUSIVE_LOCKS_REQUIRED(_data_m);

  void _do_merge(const std::shared_ptr<retention_file>& to_merge, lib& rrd_lib);

 public:
  retention_manager(const retention_config& config,
                    const std::shared_ptr<boost::asio::io_context>& io_context,
                    const std::shared_ptr<spdlog::logger>& logger);

  static std::shared_ptr<retention_manager> load(
      const retention_config& config,
      const std::shared_ptr<boost::asio::io_context>& io_context,
      const std::shared_ptr<spdlog::logger>& logger);

  static void reset();

  bool enabled() const noexcept {
    return !_config.metrics_dir.empty() || !_config.status_dir.empty();
  }

  /**
   * @brief Append a metric point to the retention file of the metric.
   */
  void write_metric(const Metric& metric);

  /**
   * @brief Append a status point to the retention file of the status.
   */
  void write_status(const Status& status);

  /**
   * @brief Remove the retention file of a metric or a status (on rebuild or
   * remove_graph event).
   */
  void remove_metric(uint64_t metric_id);
  void remove_status(uint64_t index_id);

  /**
   * @brief Stop the merge of a metric or a status if it is running, its rrd
   * file is left untouched.
   */
  void abort_metric_merge(uint64_t metric_id);
  void abort_status_merge(uint64_t index_id);

  /**
   * @brief Start the merge of a retention file if one is due and no merge is
   * running.
   *
   * @return the id of the metric or status being merged and true if it is a
   * status, nothing if no merge has been started.
   */
  std::optional<
      std::pair<uint64_t /* index_id or metric_id*/, bool /*is status*/>>
  start_merge(const std::shared_ptr<lib>& rrd_lib, backend* cached_client);
};

}  // namespace com::centreon::broker::rrd

#endif  // CCB_RRD_RETENTION_MANAGER_HH
