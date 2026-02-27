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

#ifndef CENTREON_AGENT_GRPC_CA_BOOTSTRAP_HH
#define CENTREON_AGENT_GRPC_CA_BOOTSTRAP_HH

#include <memory>
#include <optional>
#include <string>

#include <spdlog/logger.h>
#include "com/centreon/common/grpc/grpc_client.hh"
#include "config.hh"

namespace com::centreon::agent {

/**
 * @brief Orchestrates CA bootstrap from fingerprint and persists resulting TLS
 * configuration updates.
 */
class ca_bootstrap_workflow {
  const config& _conf;
  const std::string _config_path;
  const std::shared_ptr<com::centreon::common::grpc::grpc_config> _grpc_conf;
  const std::shared_ptr<spdlog::logger> _logger;

 public:
  ca_bootstrap_workflow(
      const config& conf,
      std::string config_path,
      std::shared_ptr<com::centreon::common::grpc::grpc_config> grpc_conf,
      std::shared_ptr<spdlog::logger> logger);

  void run();

 private:
  bool is_bootstrap_needed() const;
  bool probe_full_tls_connection() const;
  std::optional<std::string> fetch_ca_with_tls_skip_verify() const;
  bool validate_ca_fingerprint(const std::string& ca_pem) const;
  bool persist_ca_to_file_and_config(const std::string& ca_pem) const;
};

/**
 * @brief Thin testable wrapper exposing gRPC channel access for bootstrap RPCs.
 */
class bootstrap_grpc_client
    : public com::centreon::common::grpc::grpc_client_base {
 public:
  using com::centreon::common::grpc::grpc_client_base::grpc_client_base;

  const std::shared_ptr<::grpc::Channel>& get_channel() const {
    return _channel;
  }
};

/**
 * @brief Bootstrap the remote CA certificate from a configured fingerprint when
 * the initial TLS connection fails.
 *
 * Public behavior is intentionally stable: this function only mutates
 * `grpc_conf`/config files when bootstrap succeeds.
 */
void bootstrap_ca_from_fingerprint_if_needed(
    const config& conf,
    const std::string& config_path,
    const std::shared_ptr<com::centreon::common::grpc::grpc_config>& grpc_conf,
    const std::shared_ptr<spdlog::logger>& logger);

std::string read_file_content(const std::string& file_path,
                              const std::shared_ptr<spdlog::logger>& logger);
}  // namespace com::centreon::agent

#endif
