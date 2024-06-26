/**
 * Copyright 2011-2013, 2020-2021 Centreon
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

#ifndef CCB_RRD_CONNECTOR_HH
#define CCB_RRD_CONNECTOR_HH

#include "com/centreon/broker/io/endpoint.hh"

namespace com::centreon::broker {

namespace rrd {
/**
 *  @class connector connector.hh "com/centreon/broker/rrd/connector.hh"
 *  @brief RRD connector.
 *
 *  Generate an RRD stream that will write files.
 */
class connector : public io::endpoint {
  std::string _real_path_of(std::string const& path);

  uint32_t _cache_size;
  std::string _cached_local;
  uint16_t _cached_port;
  bool _ignore_update_errors;
  std::string _metrics_path;
  std::string _status_path;
  bool _write_metrics;
  bool _write_status;

 public:
  connector();
  connector(const connector&) = delete;
  ~connector() noexcept = default;
  connector& operator=(connector const& right) = delete;
  std::shared_ptr<io::stream> open() override;
  void set_cache_size(uint32_t cache_size);
  void set_cached_local(std::string const& local_socket);
  void set_cached_net(uint16_t port) noexcept;
  void set_ignore_update_errors(bool ignore) noexcept;
  void set_metrics_path(std::string const& metrics_path);
  void set_status_path(std::string const& status_path);
  void set_write_metrics(bool write_metrics) noexcept;
  void set_write_status(bool write_status) noexcept;
};
}  // namespace rrd

}  // namespace com::centreon::broker

#endif  // !CCB_RRD_CONNECTOR_HH
