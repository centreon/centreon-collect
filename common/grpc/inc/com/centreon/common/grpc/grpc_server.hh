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

#include <grpcpp/security/auth_metadata_processor.h>
#include <grpcpp/security/credentials.h>
#include <grpcpp/server.h>
#include <grpcpp/server_builder.h>
#include "jwt-cpp/jwt.h"

#include "com/centreon/common/grpc/grpc_config.hh"

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
  void _init(const builder_option& options);

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

class AuthProcessor : public ::grpc::AuthMetadataProcessor {
  std::shared_ptr<spdlog::logger> _logger;
  absl::flat_hash_set<std::string> _trusted_tokens;
  std::string _private_key_jwt;

 public:
  AuthProcessor(const absl::flat_hash_set<std::string>& tokens,
                const std::string& key_jwt,
                const std::shared_ptr<spdlog::logger>& logger)
      : _logger(logger) {
    // Make a copy of the tokens map
    _trusted_tokens = tokens;
    // Make a copy of the token_key string
    _private_key_jwt = key_jwt;
  }

  ::grpc::Status Process(const InputMetadata& auth_metadata,
                         ::grpc::AuthContext* context [[maybe_unused]],
                         OutputMetadata* consumed_auth_metadata
                         [[maybe_unused]],
                         OutputMetadata* response_metadata [[maybe_unused]]) {
    //  some debug code to delete:
    SPDLOG_LOGGER_INFO(_logger, "AuthProcessor::Process");
    SPDLOG_LOGGER_INFO(_logger, "token_key: {}", _private_key_jwt);
    for (const auto& token : _trusted_tokens) {
      SPDLOG_LOGGER_INFO(_logger, "tokens : {}", token);
    }
    // Extract the JWT token from the metadata
    std::string token;
    auto auth_md = auth_metadata.find("authorization");
    if (auth_md != auth_metadata.end()) {
      std::string auth_header(auth_md->second.data(), auth_md->second.size());

      SPDLOG_LOGGER_INFO(_logger, "Authorization header: {}", auth_header);

      const std::string bearer_prefix = "Bearer ";
      if (auth_header.rfind(bearer_prefix, 0) == 0) {
        token = auth_header.substr(bearer_prefix.size());
      }
      if (token.empty()) {
        std::cerr << "No JWT provided by client\n";
        return ::grpc::Status(::grpc::StatusCode::UNAUTHENTICATED,
                              "Missing JWT token");
      }
      try {
        auto decoded = jwt::decode(token);
        auto verifier = jwt::verify().allow_algorithm(
            jwt::algorithm::hs256{_private_key_jwt});
        verifier.verify(decoded);
        // Check the expiration time
        auto exp = decoded.get_expires_at();
        if (std::chrono::system_clock::now() >= exp) {
          std::cerr << "Token has expired.\n";
          std::cerr << "Expiration time: " << exp.time_since_epoch().count()
                    << "\n";
          return ::grpc::Status(::grpc::StatusCode::UNAUTHENTICATED,
                                "Token expired");
        }
        SPDLOG_LOGGER_INFO(_logger, "Token is valid and not expired.\n");
        return ::grpc::Status::OK;
      } catch (const std::exception& e) {
        std::cerr << "Invalid token: " << e.what() << "\n";
        return ::grpc::Status(::grpc::StatusCode::UNAUTHENTICATED,
                              "Invalid JWT token");
      }
    }
    std::cerr << "Authorization header is missing.\n";
    return ::grpc::Status(::grpc::StatusCode::UNAUTHENTICATED,
                          "Missing authorization metadata");
  }
  bool IsBlocking() const { return true; }
};

}  // namespace com::centreon::common::grpc

#endif
