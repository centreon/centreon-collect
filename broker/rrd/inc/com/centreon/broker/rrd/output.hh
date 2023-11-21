/*
** Copyright 2011-2013 Centreon
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
**
** For more information : contact@centreon.com
*/

#ifndef CCB_RRD_OUTPUT_HH
#define CCB_RRD_OUTPUT_HH

#include "bbdo/rebuild_message.pb.h"
#include "com/centreon/broker/io/stream.hh"

#include "com/centreon/broker/rrd/backend.hh"
#include "com/centreon/broker/rrd/cached.hh"
#include "com/centreon/broker/rrd/lib.hh"

namespace com::centreon::broker {

namespace rrd {
/**
 *  @class output output.hh "com/centreon/broker/rrd/output.hh"
 *  @brief RRD output class.
 *
 *  Write RRD files.
 */
template <typename T>
class output : public io::stream {
  using rebuild_cache =
      std::unordered_map<std::string, std::list<std::shared_ptr<io::data>>>;

  using rebuild_metric_to_index =
      boost::container::flat_map<uint64_t, uint64_t>;

  bool _ignore_update_errors;
  std::string _metrics_path;
  rebuild_cache _metrics_rebuild;
  rebuild_metric_to_index _metrics_to_index_rebuild;
  std::string _status_path;
  rebuild_cache _status_rebuild;
  const bool _write_metrics;
  const bool _write_status;
  T _backend;

  /* Loggers */
  std::shared_ptr<spdlog::logger> _logger;

  void _rebuild_data(const RebuildMessage& rm);

 public:
  output(std::string const& metrics_path,
         std::string const& status_path,
         uint32_t cache_size,
         bool ignore_update_errors,
         bool write_metrics = true,
         bool write_status = true);
  output(std::string const& metrics_path,
         std::string const& status_path,
         uint32_t cache_size,
         bool ignore_update_errors,
         std::string const& local,
         bool write_metrics = true,
         bool write_status = true);
  output(std::string const& metrics_path,
         std::string const& status_path,
         uint32_t cache_size,
         bool ignore_update_errors,
         unsigned short port,
         bool write_metrics = true,
         bool write_status = true);
  output(const output&) = delete;
  output& operator=(const output&) = delete;
  ~output() noexcept = default;
  bool read(std::shared_ptr<io::data>& d, time_t deadline) override;
  void update() override;
  int32_t write(std::shared_ptr<io::data> const& d) override;
  int32_t stop() override { return 0; }
};

}  // namespace rrd

}  // namespace com::centreon::broker

#endif  // !CCB_RRD_OUTPUT_HH
