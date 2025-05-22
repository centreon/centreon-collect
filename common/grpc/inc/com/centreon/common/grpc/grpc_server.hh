/*
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
#ifndef COMMON_GRPC_SERVER_HH
#define COMMON_GRPC_SERVER_HH

#include <grpcpp/server.h>
#include <grpcpp/server_builder.h>

#include "com/centreon/common/grpc/grpc_config.hh"
#include "com/centreon/exceptions/msg_fmt.hh"
#include "common/crypto/jwt.hh"

namespace com::centreon::common::grpc {

/**
 * @brief base class to create a grpc server
 *
 */
class grpc_server_base {
  grpc_config::pointer _conf;
  std::shared_ptr<spdlog::logger> _logger;
  std::unique_ptr<::grpc::Server> _server;

 protected:
  using builder_option = std::function<void(::grpc::ServerBuilder&)>;
  void _init(const builder_option& options, bool with_auth_process = false);

 public:
  grpc_server_base(const grpc_config::pointer& conf,
                   const std::shared_ptr<spdlog::logger>& logger);

  virtual ~grpc_server_base();

  void shutdown(const std::chrono::system_clock::duration& timeout);

  grpc_server_base(const grpc_server_base&) = delete;
  grpc_server_base& operator=(const grpc_server_base&) = delete;

  const grpc_config::pointer& get_conf() const { return _conf; }
  const std::shared_ptr<spdlog::logger>& get_logger() const { return _logger; }

  bool initialized() const { return _server.get(); }
};

class Authprocess final : public ::grpc::AuthMetadataProcessor {
  const std::shared_ptr<const absl::flat_hash_set<std::string>> _trusted_tokens;
  const std::shared_ptr<spdlog::logger> _logger;

 public:
  Authprocess(
      std::shared_ptr<const absl::flat_hash_set<std::string>> trusted_tokens,
      std::shared_ptr<spdlog::logger> logger)
      : _trusted_tokens(std::move(trusted_tokens)), _logger(std::move(logger)) {
    assert(_trusted_tokens && _logger);
  }

  ::grpc::Status Process(const InputMetadata& auth_metadata,
                         ::grpc::AuthContext* context [[maybe_unused]],
                         OutputMetadata* consumed_auth_metadata
                         [[maybe_unused]],
                         OutputMetadata* response_metadata [[maybe_unused]]) {
    // Extract the JWT token from the metadata
    auto it = auth_metadata.find("authorization");
    std::chrono::system_clock::time_point exp_time =
        std::chrono::system_clock::time_point::min();

    if (it == auth_metadata.end()) {
      SPDLOG_LOGGER_ERROR(_logger, "UNAUTHENTICATED: No authorization header");
      return ::grpc::Status(::grpc::StatusCode::UNAUTHENTICATED,
                            "Missing authorization metadata");
    }

    std::string auth_header(it->second.data(), it->second.size());
    SPDLOG_LOGGER_INFO(_logger, "Token found in Metadata");
    try {
      common::crypto::jwt jwt(auth_header);
      if (jwt.get_exp() < std::chrono::system_clock::now()) {
        SPDLOG_LOGGER_ERROR(_logger, "UNAUTHENTICATED : Token expired");
        return ::grpc::Status(::grpc::StatusCode::UNAUTHENTICATED,
                              "Token expired");
      }
      // check if token is trusted by the service
      if (!_trusted_tokens->contains(jwt.get_string())) {
        SPDLOG_LOGGER_ERROR(_logger, "UNAUTHENTICATED : Token is not trusted");
        return ::grpc::Status(::grpc::StatusCode::UNAUTHENTICATED,
                              "Token not trusted");
      }
      exp_time = jwt.get_exp();
      context->AddProperty(
          "jwt-exp",
          std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(
                             exp_time.time_since_epoch())
                             .count()));
      SPDLOG_LOGGER_INFO(_logger, "Token is valid");
      return ::grpc::Status::OK;
    } catch (const com::centreon::exceptions::msg_fmt& ex) {
      SPDLOG_LOGGER_ERROR(_logger, "Error: {}", ex.what());
      return ::grpc::Status(::grpc::StatusCode::UNAUTHENTICATED, ex.what());
    }
  }

  bool IsBlocking() const { return true; }
};
}  // namespace com::centreon::common::grpc

#endif
