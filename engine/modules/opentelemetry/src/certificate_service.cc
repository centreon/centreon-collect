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

#include "certificate_service.hh"

#include "common/crypto/cert_tree.hh"

using namespace com::centreon::engine::modules::opentelemetry::centreon_agent;

/**
 * @brief Construct a new ca certificate service object
 *
 * @param logger Logger instance for service operations
 */
certificate_service::certificate_service(
    const std::shared_ptr<spdlog::logger>& logger,
    const std::string& ca_cert)
    : _logger(logger), _ca_cert(ca_cert) {
  // calculate fingerprint
  std::unique_ptr<X509, decltype(&X509_free)> cert(
      common::crypto::cert_tree::load_cert_from_string(ca_cert), X509_free);
  if (cert) {
    _fingerprint = common::crypto::cert_tree::cert_sha(cert.get());
  }
}

/**
 * @brief Factory method to create certificate_service instance
 *
 * @param logger Logger instance
 * @return std::shared_ptr<certificate_service>
 */
std::shared_ptr<certificate_service> certificate_service::load(
    const std::shared_ptr<spdlog::logger>& logger,
    const std::string& ca_cert) {
  SPDLOG_LOGGER_INFO(logger, "certificate service loaded");

  return std::make_shared<certificate_service>(logger, ca_cert);
}

/**
 * @brief Implement GetCACertificate RPC method
 * Reads CA certificate from disk, computes fingerprint, and validates if
 * requested
 *
 * @param context gRPC server context
 * @param request CA certificate request with optional expected fingerprint
 * @param response CA certificate response with PEM and fingerprint
 * @return ::grpc::Status
 */
::grpc::Status certificate_service::GetCACertificate(
    ::grpc::ServerContext* context,
    const com::centreon::agent::CACertificateRequest* request [[maybe_unused]],
    com::centreon::agent::CACertificateResponse* response) {
  try {
    // Validate token from client metadata
    auto metadata = context->client_metadata();
    auto token_iter = metadata.find("x-token");

    if (token_iter == metadata.end()) {
      SPDLOG_LOGGER_WARN(
          _logger,
          "CA certificate request denied: missing token for the peer {}",
          context->peer());
      return ::grpc::Status(::grpc::StatusCode::UNAUTHENTICATED,
                            "Missing authentication token");
    }

    std::string client_token(token_iter->second.data(),
                             token_iter->second.size());

    if (client_token != _fingerprint) {
      SPDLOG_LOGGER_WARN(
          _logger,
          "CA certificate request denied: invalid token for the peer {}",
          context->peer());
      return ::grpc::Status(::grpc::StatusCode::PERMISSION_DENIED,
                            "Invalid authentication token");
    }

    // Set response
    response->set_certificate_pem(_ca_cert);
    SPDLOG_LOGGER_CRITICAL(
        _logger, "[SECURITY] CA bootstrap certificate delivered to peer {}",
        context->peer());

    return ::grpc::Status::OK;

  } catch (const std::exception& e) {
    SPDLOG_LOGGER_ERROR(_logger,
                        "Error retrieving CA certificate for the peer {}: {}",
                        context->peer(), e.what());
    return ::grpc::Status(
        ::grpc::StatusCode::INTERNAL,
        std::string("Error retrieving CA certificate: ") + e.what());
  }
}
