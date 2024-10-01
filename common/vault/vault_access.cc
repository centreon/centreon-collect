#include "common/vault/vault_access.hh"
#include <absl/strings/ascii.h>
#include <boost/beast.hpp>
#include <fstream>
#include <nlohmann/json.hpp>
#include "com/centreon/common/http/http_client.hh"
#include "com/centreon/common/http/http_config.hh"
#include "com/centreon/common/http/https_connection.hh"
#include "com/centreon/common/pool.hh"
#include "com/centreon/exceptions/msg_fmt.hh"
#include "common/crypto/aes256.hh"

using namespace com::centreon::common::http;
using namespace com::centreon::common::vault;
using com::centreon::common::crypto::aes256;

vault_access::vault_access(const std::string& env_file,
                           const std::string& vault_file,
                           const std::shared_ptr<spdlog::logger>& logger)
    : _logger{logger} {
  if (env_file.empty())
    set_env_informations("/usr/share/centreon/.env");
  else
    set_env_informations(env_file);

  if (_app_secret.empty())
    throw exceptions::msg_fmt("No APP_SECRET provided.");
  set_vault_informations(vault_file);

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
    connection_creator conn_creator = [client_conf, logger = _logger]() {
      auto ssl_init = [](asio::ssl::context& ctx,
                         const http_config::pointer& conf [[maybe_unused]]) {
        ctx.set_verify_mode(asio::ssl::context::verify_peer);
        ctx.set_default_verify_paths();
      };
      return https_connection::load(common::pool::io_context_ptr(), logger,
                                    client_conf, ssl_init);
    };
    auto client = client::load(common::pool::io_context_ptr(), _logger,
                               client_conf, conn_creator);
    auto req = std::make_shared<request_base>(boost::beast::http::verb::post,
                                              _url, "/v1/auth/approle/login");
    req->body() = fmt::format("{{ \"role_id\":\"{}\", \"secret_id\":\"{}\" }}",
                              _role_id, _secret_id);
    req->content_length(req->body().length());
    client->send(req, [logger = _logger](const boost::beast::error_code& err,
                                         const std::string& detail,
                                         const response_ptr& response) mutable {
      if (err && err != boost::asio::ssl::error::stream_truncated) {
        logger->error("Error from http server: {}", err.message());
      } else {
        logger->info("We got a response: detail = {} ; response = {}", detail,
                     response ? response->body() : "nullptr");
      }
    });
  }
}

void vault_access::set_vault_informations(const std::string& vault_file) {
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
  } else
    throw exceptions::msg_fmt(
        "The '{}' file is malformed, we should have keys 'salt', 'role_id', "
        "'secret_id', 'url', 'port', 'root_path'.",
        vault_file);
}

void vault_access::set_env_informations(const std::string& env_file) {
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

void vault_access::_decrypt_role_and_secret() {}
