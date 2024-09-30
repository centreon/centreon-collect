#include "vault_access.hh"
#include <nlohmann/json.hpp>
#include "aes256.hh"

using namespace com::centreon::common::vault;

void vault_access::set_vault_informations(const std::string& vault_file) {
  try {
    std::ifstream ifs(vault_file);
    nlohmann::json vault_configuration = nlohmann::json::parse(ifs);
    if (vault_configuration.contains("salt") &&
        vault_configuration.contains("role_id") &&
        vault_configuration.contains("secret_id") &&
        vault_configuration.contains("url") &&
        vault_configuration.contains("port") &&
        vault_configuration.contains("root_path")) {
      const std::string& second_key = vault_configuration["salt"];
      aes256 access(first_key, second_key);
      role_id = access.decrypt(vault_configuration["role_id"]);
      secret_id = access.decrypt(vault_configuration["secret_id"]);
      url = vault_configuration["url"];
      port = vault_configuration["port"];
      root_path = vault_configuration["root_path"];
      asio::ip::tcp::resolver resolver(common::pool::io_context());
      const auto results = resolver.resolve(url, fmt::format("{}", port));
      if (results.empty())
        _config_logger->error("Unable to resolve the vault server '{}'", url);
      else {
        http_config::pointer client_conf = std::make_shared<http_config>(
            results, url, true, std::chrono::seconds(10),
            std::chrono::seconds(30), std::chrono::seconds(30), 30,
            std::chrono::seconds(10), 5, std::chrono::hours(1), 1,
            asio::ssl::context_base::tlsv12_client);
        connection_creator conn_creator = [client_conf,
                                           logger = _config_logger]() {
          auto ssl_init = [](asio::ssl::context& ctx,
                             const http_config::pointer& conf
                             [[maybe_unused]]) {
            ctx.set_verify_mode(asio::ssl::context::verify_peer);
            ctx.set_default_verify_paths();
          };
          return https_connection::load(common::pool::io_context_ptr(), logger,
                                        client_conf, ssl_init);
        };
        auto client = client::load(common::pool::io_context_ptr(),
                                   _config_logger, client_conf, conn_creator);
        auto req = std::make_shared<request_base>(
            boost::beast::http::verb::post, url, "/v1/auth/approle/login");
        req->body() =
            fmt::format("{{ \"role_id\":\"{}\", \"secret_id\":\"{}\" }}",
                        role_id, secret_id);
        req->content_length(req->body().length());
        client->send(req, [logger = _config_logger](
                              const boost::beast::error_code& err,
                              const std::string& detail,
                              const response_ptr& response) mutable {
          if (err && err != boost::asio::ssl::error::stream_truncated) {
            logger->error("Error from http server: {}", err.message());
          } else {
            logger->info("We got a response: detail = {} ; response = {}",
                         detail, response ? response->body() : "nullptr");
          }
        });
      }
    } else
      _config_logger->error(
          "The file '{}' must contain keys 'salt', 'role_id', 'secret_id', "
          "url, port and root_path.",
          vault_file);
  } catch (const std::exception& e) {
    _config_logger->error("Error while reading '{}': {}", vault_file, e.what());
  }
}
