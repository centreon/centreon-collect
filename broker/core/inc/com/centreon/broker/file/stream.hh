/**
 * Copyright 2011-2015,2017, 2020-2023 Centreon
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

#ifndef CCB_FILE_STREAM_HH
#define CCB_FILE_STREAM_HH

#include "com/centreon/broker/file/splitter.hh"
#include "com/centreon/broker/io/stream.hh"
#include "com/centreon/broker/stats/center.hh"

namespace com::centreon::broker::file {
/**
 *  @class stream stream.hh "com/centreon/broker/file/stream.hh"
 *  @brief File stream.
 *
 *  Read and write data to a stream.
 */
class stream : public io::stream {
  splitter _splitter;
  QueueFileStats* _stats;
  std::time_t _last_stats;
  std::time_t _last_stats_perc;
  mutable long long _last_read_offset;
  mutable time_t _last_time;
  mutable long long _last_write_offset;
  std::array<std::pair<int64_t, double>, 10> _stats_perc;
  size_t _stats_idx;
  size_t _stats_size;
  std::shared_ptr<com::centreon::broker::stats::center> _center;

  void _update_stats();

 public:
  stream(const std::string& path,
         QueueFileStats* s,
         uint32_t max_file_size = 100000000u,
         bool auto_delete = false);
  ~stream() noexcept = default;
  stream(const stream&) = delete;
  stream& operator=(const stream&) = delete;
  std::string peer() const override;
  bool read(std::shared_ptr<io::data>& d, time_t deadline) override;
  void remove_all_files();
  void statistics(nlohmann::json& tree) const override;
  int32_t write(std::shared_ptr<io::data> const& d) override;
  int32_t stop() override;
  uint32_t max_file_size() const;
};
}  // namespace com::centreon::broker::file

#endif  // !CCB_FILE_STREAM_HH
