/**
 * Copyright 2024 Centreon
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

#ifndef CCE_MOD_OTL_SERVER_OTLCONFIG_HH
#define CCE_MOD_OTL_SERVER_OTLCONFIG_HH

#include "centreon_agent/agent_config.hh"
#include "grpc_config.hh"
#include "telegraf/conf_server.hh"

namespace com::centreon::engine::modules::opentelemetry {
class otl_config {
  grpc_config::pointer _grpc_conf;
  telegraf::conf_server_config::pointer _telegraf_conf_server_config;

  centreon_agent::agent_config::pointer _centreon_agent_config;

  int _max_length_grpc_log = -1;  // all otel are logged if negative
  bool _json_grpc_log = false;    // if true, otel object are logged in json
                                  // format instead of protobuf debug format

  // this two attributes are limits used by otel otl_data_point fifos
  // if fifo size exceed _max_fifo_size, oldest data_points are removed
  // Also, data_points older than _second_fifo_expiry are removed from fifos
  unsigned _second_fifo_expiry;
  size_t _max_fifo_size;

 public:
  otl_config(const std::string_view& file_path, asio::io_context& io_context);

  grpc_config::pointer get_grpc_config() const { return _grpc_conf; }
  telegraf::conf_server_config::pointer get_telegraf_conf_server_config()
      const {
    return _telegraf_conf_server_config;
  }

  centreon_agent::agent_config::pointer get_centreon_agent_config() const {
    return _centreon_agent_config;
  }

  int get_max_length_grpc_log() const { return _max_length_grpc_log; }
  bool get_json_grpc_log() const { return _json_grpc_log; }

  unsigned get_second_fifo_expiry() const { return _second_fifo_expiry; }
  size_t get_max_fifo_size() const { return _max_fifo_size; }

  bool operator==(const otl_config& right) const;

  inline bool operator!=(const otl_config& right) const {
    return !(*this == right);
  }
};

}  // namespace com::centreon::engine::modules::opentelemetry
#endif
