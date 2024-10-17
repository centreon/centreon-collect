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
#include "common/vault/vault_access.hh"
#include <absl/strings/ascii.h>
#include <absl/strings/str_split.h>
#include <nlohmann/json.hpp>
#include "com/centreon/common/http/http_config.hh"
#include "com/centreon/common/http/https_connection.hh"
#include "com/centreon/common/pool.hh"
#include "com/centreon/exceptions/msg_fmt.hh"

using namespace com::centreon::common::http;
using namespace com::centreon::common::vault;
using com::centreon::common::crypto::aes256;

/**
 * @brief Construct a new vault_access object. the constructor needs the path to
 * the env file and the path to the vault file. The vault file should contain
 * the following keys: 'salt', 'role_id', 'secret_id', 'url', 'port',
 * 'root_path'. The env file should contain the key 'APP_SECRET'.
 * The parameter verify_peer is used to verify the certificate of the vault
 * server.
 *
 * @param env_file The path to the env file.
 * @param vault_file The path to the vault file.
 * @param verify_peer A boolean to verify the certificate of the vault server.
 * @param logger The logger.
 */
vault_access::vault_access(const std::string& env_file,
                           const std::string& vault_file,
                           bool verify_peer,
                           const std::shared_ptr<spdlog::logger>& logger)
    : _logger{logger} {
  if (env_file.empty())
    _set_env_informations("/usr/share/centreon/.env");
  else
    _set_env_informations(env_file);

  if (_app_secret.empty())
    throw exceptions::msg_fmt("No APP_SECRET provided.");
  _set_vault_informations(vault_file);

  _aes_encryptor = std::make_unique<aes256>(_app_secret, _salt);

  _role_id = _aes_encryptor->decrypt(_role_id);
  _secret_id = _aes_encryptor->decrypt(_secret_id);

  asio::ip::tcp::resolver resolver(common::pool::io_context());
  const auto results = resolver.resolve(_url, fmt::format("{}", _port));
  if (results.empty())
    throw exceptions::msg_fmt("Unable to resolve the vault server '{}'", _url);
  else {
    http_config::pointer client_conf = std::make_shared<http_config>(
        results, _url, true, std::chrono::seconds(10), std::chrono::seconds(30),
        std::chrono::seconds(30), 30, std::chrono::seconds(10), 5,
        std::chrono::hours(1), 1, asio::ssl::context_base::tlsv12_client);
    client_conf->set_verify_peer(verify_peer);
    connection_creator conn_creator = [client_conf, logger = _logger]() {
      auto ssl_init = [](asio::ssl::context& ctx,
                         const http_config::pointer& conf [[maybe_unused]]) {
        if (conf->verify_peer())
          ctx.set_verify_mode(asio::ssl::context::verify_peer);
        else
          ctx.set_verify_mode(asio::ssl::context::verify_none);
        ctx.set_default_verify_paths();
      };
      return https_connection::load(common::pool::io_context_ptr(), logger,
                                    client_conf, ssl_init);
    };
    _client = client::load(common::pool::io_context_ptr(), _logger, client_conf,
                           conn_creator);
  }
}

/**
 * @brief Read the vaul file and set its informations in the object. Throw an
 * exception if the file could not be open or if the file is malformed.
 *
 * @param vault_file The path to the vault file.
 */
void vault_access::_set_vault_informations(const std::string& vault_file) {
  std::ifstream ifs(vault_file);
  nlohmann::json vault_configuration = nlohmann::json::parse(ifs);
  if (vault_configuration.contains("salt") &&
      vault_configuration.contains("role_id") &&
      vault_configuration.contains("secret_id") &&
      vault_configuration.contains("url") &&
      vault_configuration.contains("port") &&
      vault_configuration.contains("root_path")) {
    _salt = vault_configuration["salt"];
    _url = vault_configuration["url"];
    _port = vault_configuration["port"];
    _root_path = vault_configuration["root_path"];
    _role_id = vault_configuration["role_id"];
    _secret_id = vault_configuration["secret_id"];
  } else
    throw exceptions::msg_fmt(
        "The '{}' file is malformed, we should have keys 'salt', 'role_id', "
        "'secret_id', 'url', 'port', 'root_path'.",
        vault_file);
}

/**
 * @brief Read the env file and set the APP_SECRET in the object. Throw an
 * exception if the file could not be open.
 *
 * @param env_file The path to the env file.
 */
void vault_access::_set_env_informations(const std::string& env_file) {
  std::ifstream ifs(env_file);
  if (ifs.is_open()) {
    std::string line;
    while (std::getline(ifs, line)) {
      if (line.find("APP_SECRET=") == 0) {
        _app_secret = line.substr(11);
        _app_secret = absl::StripAsciiWhitespace(_app_secret);
        break;
      }
    }
  } else
    throw exceptions::msg_fmt("The env file could not be open");
}

/**
 * @brief Decrypt the input string using the AES256 algorithm. Throw an
 * exception if the input string is not stored in the vault.
 *
 * @param encrypted The string to decrypt.
 *
 * @return The decrypted string.
 */
std::string vault_access::decrypt(const std::string& encrypted) {
  std::string_view head = encrypted;
  if (head.substr(0, 25) != "secret::hashicorp_vault::") {
    _logger->debug("Password is not stored in the vault");
    return encrypted;
  } else
    head.remove_prefix(25);

  /* We get the token */
  auto req = std::make_shared<request_base>(boost::beast::http::verb::post,
                                            _url, "/v1/auth/approle/login");
  req->body() = fmt::format("{{ \"role_id\":\"{}\", \"secret_id\":\"{}\" }}",
                            _role_id, _secret_id);
  req->content_length(req->body().length());

  std::promise<std::string> promise;
  std::future<std::string> future = promise.get_future();
  _client->send(req, [logger = _logger, &promise](
                         const boost::beast::error_code& err,
                         const std::string& detail [[maybe_unused]],
                         const response_ptr& response) mutable {
    if (err && err != boost::asio::ssl::error::stream_truncated) {
      auto exc = std::make_exception_ptr(
          exceptions::msg_fmt("Error from http server: {}", err.message()));
      promise.set_exception(exc);
    } else {
      nlohmann::json resp = nlohmann::json::parse(response->body());
      std::string token = resp["auth"]["client_token"].get<std::string>();
      promise.set_value(std::move(token));
    }
  });

  std::pair<std::string_view, std::string_view> p =
      absl::StrSplit(head, absl::ByString("::"));

  std::string token(future.get());
  req = std::make_shared<request_base>(boost::beast::http::verb::get, _url,
                                       fmt::format("/v1/{}", p.first));
  req->set("X-Vault-Token", token);
  std::promise<std::string> promise_decrypted;
  std::future<std::string> future_decrypted = promise_decrypted.get_future();
  _client->send(
      req, [logger = _logger, &promise_decrypted, field = p.second](
               const boost::beast::error_code& err, const std::string& detail,
               const response_ptr& response) mutable {
        if (err && err != boost::asio::ssl::error::stream_truncated) {
          logger->error("Error from http server: {}", err.message());
          auto exc = std::make_exception_ptr(
              exceptions::msg_fmt("Error from http server: {}", err.message()));
          promise_decrypted.set_exception(exc);
        } else {
          logger->info("We got a the result: detail = {} ; response = {}",
                       detail, response ? response->body() : "nullptr");
          try {
            nlohmann::json resp = nlohmann::json::parse(response->body());
            std::string result = resp["data"]["data"][field];
            promise_decrypted.set_value(result);
          } catch (const std::exception& e) {
            auto exc = std::make_exception_ptr(exceptions::msg_fmt(
                "Response is not as expected: {}", err.message()));
            promise_decrypted.set_exception(exc);
          }
        }
      });
  return future_decrypted.get();
}
