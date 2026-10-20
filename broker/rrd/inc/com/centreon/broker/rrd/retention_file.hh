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
#include "com/centreon/broker/rrd/internal.hh"

namespace com::centreon::broker::rrd {

class retention_file {
 public:
  struct slot {
    uint64_t ts;
    union {
      double d_value;
      int64_t i_value;
    } val;
  };

 private:
  struct header {
    int64_t id;
    bool is_status;
    Metric::ValueType value_type;
    uint64_t rrd_len;   // in second
    uint64_t interval;  // in second
    uint64_t first_ts;  // ts of oldest slot
    uint64_t last_ts;   // ts of latest slot
  };

  struct file_content : public header {
    slot data[];  // variable array circular buffer
                  // len=(rrd_len+interval-1)/interval
  };

  std::filesystem::path _file_path;
  header _header;

  void _create_file();

  void _grow_file(int fd);

  void _append(const slot& to_append);

 public:
  class mapped_slot {
    file_content* _data;
    size_t _file_size;

    const slot *_begin, *_end;

   public:
    mapped_slot(const std::filesystem::path& path);
    mapped_slot(const mapped_slot&) = delete;
    mapped_slot& operator=(const mapped_slot&) = delete;
    ~mapped_slot();

    const slot* begin() const { return _begin; }
    const slot* end() const { return _end; }
  };

  retention_file(const std::filesystem::path& rrd_data_dir,
                 const Metric& first_metric);
  retention_file(const std::filesystem::path& rrd_data_dir,
                 const Status& first_status);

  retention_file(const std::filesystem::path& file_path);

  void add_metric(const Metric& metric);
  void add_status(const Status& status);

  uint64_t get_id() const { return _header.id; }
  bool is_status() const { return _header.is_status; }
  uint64_t rrd_len() const { return _header.rrd_len; }
  uint64_t interval() const { return _header.interval; }
  uint64_t first_ts() const { return _header.first_ts; }
  uint64_t last_ts() const { return _header.last_ts; }

  void sort_slots();
  const mapped_slot get_slots() const;
};

}  // namespace com::centreon::broker::rrd

#endif
