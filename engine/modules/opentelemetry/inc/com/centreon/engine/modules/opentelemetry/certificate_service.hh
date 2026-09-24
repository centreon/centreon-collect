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

#ifndef CCE_MOD_OTL_CERTIFICATE_SERVICE_HH
#define CCE_MOD_OTL_CERTIFICATE_SERVICE_HH

#include <grpcpp/support/server_callback.h>
#include <spdlog/spdlog.h>
#include "centreon_agent/agent.grpc.pb.h"

namespace com::centreon::engine::modules::opentelemetry::centreon_agent {

/**
 * @brief Dedicated gRPC service for CA certificate retrieval
 * Provides the GetCACertificate RPC method
 */
class certificate_service final : public agent::CACertificateService::Service {
  std::shared_ptr<spdlog::logger> _logger;
  const std::string _ca_cert;
  std::string _fingerprint;

 public:
  certificate_service(const std::shared_ptr<spdlog::logger>& logger,
                      const std::string& ca_cert);

  static std::shared_ptr<certificate_service> load(
      const std::shared_ptr<spdlog::logger>& logger,
      const std::string& ca_cert);

  ::grpc::Status GetCACertificate(
      ::grpc::ServerContext* context,
      const com::centreon::agent::CACertificateRequest* request,
      com::centreon::agent::CACertificateResponse* response) override;
};

}  // namespace com::centreon::engine::modules::opentelemetry::centreon_agent

#endif
