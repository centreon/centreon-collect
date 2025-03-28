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
#include "com/centreon/exceptions/msg_fmt.hh"
#include "common/crypto/jwt.hh"

#include "com/centreon/common/grpc/grpc_config.hh"
#include "spdlog/spdlog.h"

using namespace com::centreon::exceptions;

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

 public:
  AuthProcessor(const absl::flat_hash_set<std::string>& tokens,
                const std::shared_ptr<spdlog::logger>& logger)
      : _logger(logger) {
    _trusted_tokens = tokens;
  }

  ::grpc::Status Process(const InputMetadata& auth_metadata,
                         ::grpc::AuthContext* context [[maybe_unused]],
                         OutputMetadata* consumed_auth_metadata
                         [[maybe_unused]],
                         OutputMetadata* response_metadata [[maybe_unused]]) {
    // Extract the JWT token from the metadata
    auto auth_md = auth_metadata.find("authorization");
    if (auth_md != auth_metadata.end()) {
      std::string auth_header(auth_md->second.data(), auth_md->second.size());
      SPDLOG_LOGGER_INFO(_logger, "Token found in Metadata");
      try {
        common::crypto::jwt jwt(auth_header);

        // check if not expired
        if (jwt.get_exp() < std::chrono::system_clock::now()) {
          SPDLOG_LOGGER_ERROR(_logger, "UNAUTHENTICATED : Token expired");
          return ::grpc::Status(::grpc::StatusCode::UNAUTHENTICATED,
                                "Token expired");
        }

        // check if token is trusted
        if (_trusted_tokens.find(jwt.get_string()) == _trusted_tokens.end()) {
          SPDLOG_LOGGER_ERROR(_logger,
                              "UNAUTHENTICATED : Token is not trusted");
          return ::grpc::Status(::grpc::StatusCode::UNAUTHENTICATED,
                                "Token is not trusted");
        }

        SPDLOG_LOGGER_INFO(_logger, "Token is valid");
        // Attach expiration as property to AuthContext
        // context->AddProperty("jwt_expiration", jwt.get_exp_str());

        // SPDLOG_LOGGER_INFO(_logger, "Authenticated token, expiration saved:
        // {}",
        //                    jwt.get_exp_str());

        // // Indicate metadata was successfully processed
        // consumed_auth_metadata->insert(std::make_pair(
        //     std::string(auth_md->first.data(), auth_md->first.length()),
        //     std::string(auth_md->second.data(), auth_md->second.length())));

        return ::grpc::Status::OK;

      } catch (const msg_fmt& ex) {
        SPDLOG_LOGGER_ERROR(_logger, "UNAUTHENTICATED : Invalid token : {}",
                            ex.what());
        return ::grpc::Status(::grpc::StatusCode::UNAUTHENTICATED,
                              "Invalid token");
      }
    } else {
      SPDLOG_LOGGER_ERROR(_logger, "Authorization header is missing.");
      return ::grpc::Status(::grpc::StatusCode::UNAUTHENTICATED,
                            "Missing authorization metadata");
    }
  }
  bool IsBlocking() const { return true; }
};

}  // namespace com::centreon::common::grpc

#endif
