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

#ifndef CCB_RRD_RETENTION_FILE_HH
#define CCB_RRD_RETENTION_FILE_HH

#include "bbdo/storage.pb.h"
#include "com/centreon/broker/rrd/lib.hh"

namespace com::centreon::broker::rrd {

/**
 * @class retention_file
 * @brief File of the metric or status points older than the last update of
 * their rrd file, merged later into it.
 *
 * The file contains a header followed by the samples in their arrival order.
 * It is only appended to, the samples are sorted at the merge.
 */
class retention_file {
  struct header {
    int64_t id;
    bool is_status;
    Metric::ValueType value_type;
    uint64_t rrd_len;   // in second
    uint64_t interval;  // in second
  };

  struct sample {
    uint64_t ts;
    union {
      double d_value;
      int64_t i_value;
    } val;
  };

  struct file_content : public header {
    sample data[];  // samples in arrival order, their number is deduced from
                    // the file size
  };

  std::filesystem::path _file_path;
  header _header;
  time_t _last_activity;
  size_t _nb_samples;
  uint64_t _first_ts;  // ts of oldest sample
  uint64_t _last_ts;   // ts of latest sample
  // the file can't be merged before this time (set after a failed merge)
  time_t _no_merge_before = 0;

  std::atomic_bool _abort_merge = false;

  void _create_file();

  void _append(const sample& to_append);

  rrd_existing_data _get_rrd_params(const std::filesystem::path& rrd_path,
                                    lib& rrd_lib) const;

  void _to_rrd_file(const std::filesystem::path& dst_path,
                    lib& rrd_lib,
                    const rrd_existing_data& rrd_params);

 public:
  retention_file(const std::filesystem::path& rrd_data_dir,
                 const Metric& first_metric);
  retention_file(const std::filesystem::path& rrd_data_dir,
                 const Status& first_status);

  retention_file(const std::filesystem::path& file_path);

  void add_metric(const Metric& metric);
  void add_status(const Status& status);

  uint64_t id() const { return _header.id; }
  const std::filesystem::path file_path() const { return _file_path; }
  bool is_status() const { return _header.is_status; }
  uint64_t rrd_len() const { return _header.rrd_len; }
  uint64_t interval() const { return _header.interval; }
  uint64_t first_ts() const { return _first_ts; }
  uint64_t last_ts() const { return _last_ts; }

  time_t last_activity() const { return _last_activity; }
  void merge(const std::filesystem::path& rrd_path, lib& rrd_lib);

  void abort_merge() { _abort_merge = true; }
  size_t nb_samples() const { return _nb_samples; }

  time_t no_merge_before() const { return _no_merge_before; }
  void set_no_merge_before(time_t new_barrier) {
    _no_merge_before = new_barrier;
  }
};

}  // namespace com::centreon::broker::rrd

#endif
