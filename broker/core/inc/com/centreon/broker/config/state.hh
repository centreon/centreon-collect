/**
 * Copyright 2011-2012,2017, 2021-2024 Centreon
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

#ifndef CCB_CONFIG_STATE_HH
#define CCB_CONFIG_STATE_HH

#include "bbdo/bbdo/bbdo_version.hh"
#include "com/centreon/broker/config/endpoint.hh"
#include "common/log_v2/config.hh"

namespace com::centreon::broker::config {

/**
 *  @class state state.hh "com/centreon/broker/config/state.hh"
 *  @brief Full configuration state.
 *
 *  A fully parsed configuration is represented within this class
 *  which holds mandatory parameters as well as optional parameters,
 *  along with object definitions.
 */
class state {
  int _broker_id;
  uint16_t _rpc_port;
  std::string _listen_address;
  std::string _broker_name;
  uint64_t _event_queues_total_size = 0u;
  bbdo::bbdo_version _bbdo_version;
  std::string _cache_directory;
  std::string _command_file;
  std::string _command_protocol;
  std::list<endpoint> _endpoints;
  int _event_queue_max_size;
  std::string _module_dir;
  std::list<std::string> _module_list;
  std::map<std::string, std::string> _params;
  int _poller_id;
  std::string _poller_name;
  size_t _pool_size;

  /* The directory where the engine configuration files are stored. This file
   * has a sense only for the cbmod (usual value: /etc/centreon-engine) */
  std::string _engine_config_dir;

  /* The directory where php writes the pollers configurations. */
  std::string _cache_config_dir;

  /* The directory where broker stores all the pollers configurations. */
  std::string _pollers_config_dir;

  common::log_v2::config _log_conf;

 public:
  /**
   * @brief Structure used by stats_exporter_conf. It is just a pair:
   * * protocol: a value among GRPC or HTTP.
   * * url: a URL to the opentelemetry server.
   *
   */
  struct stats_exporter {
    enum exporter_protocol {
      GRPC,
      HTTP,
    };
    exporter_protocol protocol;
    std::string url;
    stats_exporter(exporter_protocol protocol, std::string&& url)
        : protocol(protocol), url(url) {}
  };

 private:
  /**
   * @brief The configuration of the stats_exporter module which is an exporter
   * to opentelemetry. We allow several exporters, that's why the
   * stats_exporters are stored in a list. All these exporters are cadenced by
   * the same attributes: export_interval that tells the duration between two
   * exports and export_timeout that tells specifies the timeout in case of
   * lock. These durations are in seconds.
   */
  struct stats_exporter_conf {
    double export_interval;
    double export_timeout;
    std::list<stats_exporter> exporters;
  } _stats_exporter_conf;

 public:
  state();
  state(state const& other);
  ~state();
  state& operator=(state const& other);
  void broker_id(int id) noexcept;
  int broker_id() const noexcept;
  void rpc_port(uint16_t port) noexcept;
  uint16_t rpc_port(void) const noexcept;
  void listen_address(const std::string& listen_address) noexcept;
  const std::string& listen_address() const noexcept;
  void broker_name(const std::string& name);
  const std::string& broker_name() const noexcept;
  void event_queues_total_size(uint64_t size);
  uint64_t event_queues_total_size() const noexcept;
  void set_bbdo_version(bbdo::bbdo_version v);
  bbdo::bbdo_version get_bbdo_version() const noexcept;
  void cache_directory(const std::string& dir);
  const std::string& cache_directory() const noexcept;
  void command_file(const std::string& file);
  const std::string& command_file() const noexcept;
  void command_protocol(const std::string& prot);
  const std::string& command_protocol() const noexcept;
  void clear();
  void add_endpoint(endpoint&& out) noexcept;
  std::list<endpoint> const& endpoints() const noexcept;
  void event_queue_max_size(int val) noexcept;
  int event_queue_max_size() const noexcept;
  const std::string& module_directory() const noexcept;
  void module_directory(const std::string& dir);
  std::list<std::string>& module_list() noexcept;
  void add_module(std::string module);
  std::list<std::string> const& module_list() const noexcept;
  std::map<std::string, std::string>& params() noexcept;
  const std::map<std::string, std::string>& params() const noexcept;
  void poller_id(int id) noexcept;
  int poller_id() const noexcept;
  void pool_size(int size) noexcept;
  int pool_size() const noexcept;
  void poller_name(const std::string& name);
  const std::string& poller_name() const noexcept;
  void set_engine_config_dir(const std::string& dir);
  const std::string& engine_config_dir() const noexcept;
  void set_cache_config_dir(const std::string& config_cache_dir);
  const std::string& cache_config_dir() const noexcept;
  void set_pollers_config_dir(const std::string& pollers_config_dir);
  const std::string& pollers_config_dir() const noexcept;
  common::log_v2::config& mut_log_conf();
  const common::log_v2::config& log_conf() const;
  stats_exporter_conf& mut_stats_exporter();
  const stats_exporter_conf& get_stats_exporter() const;
};
}  // namespace com::centreon::broker::config

#endif  // !CCB_CONFIG_STATE_HH
