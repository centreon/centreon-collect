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

#include "com/centreon/broker/remote_api.hh"

#include "broker/core/bbdo/internal.hh"
#include "broker/core/bbdo/remote.hh"
#include "com/centreon/broker/config/applier/state.hh"
#include "com/centreon/broker/misc/uuid.hh"
#include "com/centreon/common/http/http_server.hh"
#include "com/centreon/common/http/https_connection.hh"
#include "com/centreon/common/pool.hh"
#include "com/centreon/common/utf8.hh"
#include "common/crypto/aes256.hh"
#include "common/log_v2/log_v2.hh"

#include <absl/strings/ascii.h>
#include <absl/strings/escaping.h>
#include <absl/strings/match.h>
#include <absl/strings/str_join.h>
#include <absl/strings/str_split.h>
#include <algorithm>
#include <openssl/evp.h>
#include <csignal>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>
#include <unistd.h>
#include <nlohmann/json.hpp>

using namespace com::centreon::broker;
using com::centreon::common::log_v2::log_v2;
namespace http = com::centreon::common::http;
using json = nlohmann::json;

/* Defined in main.cc, loaded from engine-context.json (as db_password). */
extern std::shared_ptr<com::centreon::common::crypto::aes256>
    credentials_decrypt;

namespace {

constexpr size_t chunk_size = 1u << 20;
constexpr int default_command_timeout_ms = 10000;
constexpr int max_command_timeout_ms = 30000;
/* Below the 30 s budget of the web, its own overhead included. */
constexpr time_t deployment_progress_timeout = 20;
constexpr time_t deployment_keep_duration = 3600;
/* RemoteResult index of a plugin install outcome (cbmod). */
constexpr int32_t plugins_result_index = -2;

/* Remote executions (contracts.md §12). */
constexpr time_t exec_progress_timeout = 45;
constexpr time_t exec_start_timeout = 600;
constexpr time_t exec_deadline_margin = 60;
constexpr time_t exec_keep_duration = 3600;
constexpr size_t exec_max_pending = 256;
constexpr size_t exec_max_kept_output = 64u << 20;
constexpr size_t exec_max_output = 16u << 20;
constexpr size_t exec_max_args = 256;
constexpr size_t exec_max_argv_bytes = 64u << 10;
constexpr uint32_t exec_max_timeout_s = 3600;

/* Plugin installs on request (contracts.md §14). */
constexpr size_t install_max_packages = 16;
constexpr size_t install_max_field = 128;
constexpr size_t install_max_id = 64;
constexpr uint32_t install_default_timeout_s = 600;
constexpr uint32_t install_max_timeout_s = 1800;
constexpr time_t install_start_timeout = 1800;
constexpr time_t install_deadline_margin = 60;
constexpr time_t install_keep_duration = 3600;
constexpr size_t install_max_pending = 256;

/* Status names of the HTTP contract. */
constexpr const char* st_accepted = "ACCEPTED";
constexpr const char* st_running = "RUNNING";
constexpr const char* st_done = "DONE";
constexpr const char* st_failed = "FAILED";
constexpr const char* st_not_connected = "NOT_CONNECTED";
constexpr const char* st_peer_too_old = "PEER_TOO_OLD";
constexpr const char* st_transferring = "TRANSFERRING";
constexpr const char* st_delivered = "DELIVERED";
constexpr const char* st_applied = "APPLIED";
constexpr const char* st_rejected = "REJECTED";
constexpr const char* st_timeout = "TIMEOUT";
constexpr const char* st_installed = "INSTALLED";
constexpr const char* st_partial = "PARTIAL";

int status_weight(const std::string& s) {
  if (s == st_rejected)
    return 5;
  if (s == st_not_connected)
    return 4;
  if (s == st_timeout)
    return 3;
  if (s == st_delivered)
    return 2;
  if (s == st_applied)
    return 1;
  return 0;
}

bool is_final(const std::string& s) {
  return s == st_applied || s == st_rejected || s == st_timeout ||
         s == st_not_connected || s == st_peer_too_old;
}

bool install_is_final(const std::string& s) {
  return s != st_accepted && s != st_running;
}

bool exec_is_final(const std::string& s) {
  return s == st_done || s == st_rejected || s == st_timeout ||
         s == st_failed || s == st_not_connected || s == st_peer_too_old;
}

/**
 * @brief Read a poller id given as a JSON number or a decimal string.
 */
bool read_poller_id(const json& j, uint64_t& id) {
  if (j.is_number_unsigned()) {
    id = j.get<uint64_t>();
    return id > 0;
  }
  if (j.is_number_integer()) {
    int64_t v = j.get<int64_t>();
    if (v <= 0)
      return false;
    id = static_cast<uint64_t>(v);
    return true;
  }
  if (j.is_string())
    return absl::SimpleAtoi(j.get<std::string>(), &id) && id > 0;
  return false;
}

/**
 * @brief A relative path without "..", without leading '/', not empty.
 */
bool valid_relative_path(const std::string& p) {
  if (p.empty() || p[0] == '/' || p.find('\0') != std::string::npos)
    return false;
  for (std::string_view part : absl::StrSplit(p, '/')) {
    if (part.empty() || part == "." || part == "..")
      return false;
  }
  return true;
}

bool valid_sha256(const std::string& s) {
  if (s.size() != 64)
    return false;
  for (char c : s)
    if (!std::isxdigit(static_cast<unsigned char>(c)))
      return false;
  return true;
}

/**
 * @brief Compare two strings in a time independent of their content.
 */
bool same_secret(const std::string& a, const std::string& b) {
  unsigned char diff = a.size() == b.size() ? 0 : 1;
  for (size_t i = 0; i < a.size(); ++i)
    diff |= static_cast<unsigned char>(a[i]) ^
            static_cast<unsigned char>(i < b.size() ? b[i] : 0);
  return diff == 0;
}

using answer_function = std::function<void(http::response_ptr)>;

class api;
std::shared_ptr<api> g_api;
absl::Mutex g_api_m;

/**
 * @brief The remote API: HTTP handlers, pending requests, BBDO results.
 */
class api : public std::enable_shared_from_this<api> {
 public:
  struct conf {
    std::string listen_address = "localhost";
    uint16_t port = 8099;
    std::string token;
    std::string public_cert;
    std::string private_key;
    std::string ca_certificate = "/etc/centreon-broker/certs/ca.crt";
    uint64_t max_body_size = 64u << 20;
    /* uid of the engine running next to this cbd (env CENTRAL_POLLER_UID),
     * 0 when unknown. */
    uint64_t local_poller_uid = 0;
    /* Allow executions on a stream without a verified TLS peer (Robot). */
    bool exec_insecure = false;
  };

 private:
  struct command_item {
    uint64_t poller_id = 0;
    std::string status;
    std::string message;
    /* status, message per command */
    std::vector<std::pair<std::string, std::string>> results;
    size_t pending = 0;
  };

  struct command_request {
    std::string id;
    std::vector<command_item> items;
    std::unique_ptr<asio::steady_timer> timer;
    answer_function answer;
    bool done = false;
  };

  struct deployment {
    std::string id;
    uint64_t poller_id = 0;
    std::string action;
    std::string status;
    std::string message;
    std::string config_version;
    std::vector<std::string> plugins_missing;
    /* Plugin install outcome (index -2 results), null until the first. */
    json plugins;
    time_t last_progress = 0;
    /* At least one "broker" file: the central cbd reloads itself when this
     * deployment targets its own poller. */
    bool broker_files = false;
    /* The target engine runs next to this cbd: same poller id, or its uid
     * is CENTRAL_POLLER_UID. */
    bool own_poller = false;
    time_t finished_at = 0;
  };

  /* A remote execution. argv is never kept: it is pushed and forgotten. */
  struct execution {
    std::string id;
    uint64_t poller_id = 0;
    std::string program;  // basename of argv[0], for the logs
    uint32_t timeout_s = 300;
    std::string status;
    std::string message;
    bool has_exit_code = false;
    int32_t exit_code = 0;
    std::string output;
    uint64_t output_bytes = 0;
    bool truncated = false;
    std::string stderr_tail;
    uint32_t last_seq = 0;
    time_t accepted_at = 0;
    time_t started_at = 0;
    time_t last_progress = 0;
    time_t finished_at = 0;
  };

  /* A plugin install request (§14): packages only, no secret. */
  struct plugin_install {
    std::string id;
    uint64_t poller_id = 0;
    std::string names;  // requested packages, for the logs
    uint32_t timeout_s = install_default_timeout_s;
    std::string status;
    std::string message;
    json packages = json::array();
    time_t accepted_at = 0;
    time_t started_at = 0;
    time_t updated_at = 0;
    time_t finished_at = 0;
  };

  const conf _conf;
  std::shared_ptr<spdlog::logger> _logger;
  std::shared_ptr<asio::io_context> _io_context;
  http::server::pointer _server;
  std::unique_ptr<asio::steady_timer> _housekeeping;

  mutable absl::Mutex _m;
  /* wire request id (sent to the poller) -> request and item index */
  absl::flat_hash_map<std::string,
                      std::pair<std::shared_ptr<command_request>, size_t>>
      _commands ABSL_GUARDED_BY(_m);
  absl::flat_hash_map<std::string, std::shared_ptr<deployment>> _deployments
      ABSL_GUARDED_BY(_m);
  absl::flat_hash_map<std::string, std::shared_ptr<execution>> _executions
      ABSL_GUARDED_BY(_m);
  absl::flat_hash_map<std::string, std::shared_ptr<plugin_install>> _installs
      ABSL_GUARDED_BY(_m);
  /* Last plugin install outcome per peer, kept across reconnects. */
  absl::flat_hash_map<uint64_t, json> _plugins ABSL_GUARDED_BY(_m);
  absl::flat_hash_map<uint64_t, std::string> _config_versions
      ABSL_GUARDED_BY(_m);
  bool _stopped ABSL_GUARDED_BY(_m) = false;

  static http::response_ptr _response(boost::beast::http::status code,
                                      const json& body);
  static http::response_ptr _error(boost::beast::http::status code,
                                   const std::string& status,
                                   const std::string& message);

  void _post_commands(const json& body, answer_function&& answer);
  void _finish_command(const std::shared_ptr<command_request>& req)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(_m);
  json _command_json(const command_request& req) const;
  void _post_deployment(const json& body, answer_function&& answer);
  json _deployment_json(const deployment& d) const;
  json _pollers_json() const;
  bool _ca_json(json& out) const;
  void _post_execution(const json& body, answer_function&& answer);
  json _execution_json(const execution& e) const;
  void _finish_execution(execution& e,
                         const char* status,
                         const std::string& message,
                         time_t now) ABSL_EXCLUSIVE_LOCKS_REQUIRED(_m);
  void _on_exec_result(uint64_t poller_id, const RemoteExecResult& r);
  void _post_plugin_install(const json& body, answer_function&& answer);
  json _install_json(const plugin_install& i) const;
  void _finish_install(plugin_install& i,
                       const char* status,
                       const std::string& message,
                       time_t now) ABSL_EXCLUSIVE_LOCKS_REQUIRED(_m);
  bool _on_install_result(uint64_t poller_id, const RemoteResult& r)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(_m);
  void _on_result(uint64_t poller_id, const RemoteResult& r);
  void _on_disconnect(uint64_t poller_id);
  void _start_housekeeping();
  void _housekeep();

 public:
  api(const conf& c);
  void start();
  void stop();
  void handle(const std::shared_ptr<http::request_type>& request,
              answer_function&& answer);
};

/**
 * @brief HTTP(S) session: reads requests and hands them to the api.
 */
template <class connection_class>
class session : public connection_class {
  std::weak_ptr<api> _api;

  void _wait_for_request() {
    connection_class::receive_request(
        [me = shared_from_this()](
            const boost::beast::error_code& err, const std::string&,
            const std::shared_ptr<http::request_type>& request) {
          if (err)
            return;
          auto a = me->_api.lock();
          if (!a)
            return;
          a->handle(request, [me](http::response_ptr resp) {
            asio::post(*me->_io_context, [me, resp]() {
              me->answer(resp, [me](const boost::beast::error_code& err,
                                    const std::string& detail) {
                if (err) {
                  SPDLOG_LOGGER_ERROR(me->_logger,
                                      "remote api: fail to answer {}: {}",
                                      err.message(), detail);
                  return;
                }
                me->_wait_for_request();
              });
            });
          });
        });
  }

 public:
  using my_type = session<connection_class>;

  session(const std::shared_ptr<asio::io_context>& io_context,
          const std::shared_ptr<spdlog::logger>& logger,
          const http::http_config::pointer& conf,
          const http::ssl_ctx_initializer& ssl_initializer,
          const std::weak_ptr<api>& a)
      : connection_class(io_context, logger, conf, ssl_initializer), _api(a) {}

  std::shared_ptr<my_type> shared_from_this() {
    return std::static_pointer_cast<my_type>(
        connection_class::shared_from_this());
  }

  void on_accept() override {
    connection_class::_on_accept(
        [me = shared_from_this()](const boost::beast::error_code& err,
                                  const std::string&) {
          if (!err)
            me->_wait_for_request();
        });
  }
};

api::api(const conf& c)
    : _conf(c),
      _logger(log_v2::instance().get(log_v2::CORE)),
      _io_context(com::centreon::common::pool::io_context_ptr()) {}

/**
 * @brief Register the BBDO handlers and start the HTTP(S) server.
 */
void api::start() {
  std::weak_ptr<api> weak = shared_from_this();
  auto& reg = bbdo::remote_registry::instance();
  reg.set_result_handler([weak](uint64_t poller_id, const RemoteResult& r) {
    auto a = weak.lock();
    if (a)
      a->_on_result(poller_id, r);
  });
  reg.set_exec_result_handler(
      [weak](uint64_t poller_id, const RemoteExecResult& r) {
        auto a = weak.lock();
        if (a)
          a->_on_exec_result(poller_id, r);
      });
  reg.set_disconnect_handler([weak](uint64_t poller_id) {
    auto a = weak.lock();
    if (a)
      a->_on_disconnect(poller_id);
  });

  std::string addr = _conf.listen_address;
  if (addr == "localhost")
    addr = "127.0.0.1";
  asio::ip::tcp::endpoint ep(asio::ip::make_address(addr), _conf.port);
  bool crypted = !_conf.public_cert.empty() && !_conf.private_key.empty();
  auto http_conf = std::make_shared<http::http_config>(
      ep, "", crypted, std::chrono::seconds(10), std::chrono::seconds(30),
      std::chrono::seconds(60), 30, std::chrono::seconds(10), 0,
      std::chrono::hours(1), 10, asio::ssl::context::tlsv12, _conf.public_cert,
      _conf.private_key);
  http_conf->set_max_request_body_size(_conf.max_body_size);
  /* The HTTP layer traces whole requests and responses: never below debug,
   * so that execution argv and outputs are not logged (contracts.md §12). */
  std::shared_ptr<spdlog::logger> http_logger = _logger->clone("core");
  if (http_logger->level() < spdlog::level::debug)
    http_logger->set_level(spdlog::level::debug);

  if (crypted)
    _server = http::server::load(
        _io_context, http_logger, http_conf,
        [http_conf, io_ctx = _io_context, logger = http_logger, weak]() {
          return std::make_shared<session<http::https_connection>>(
              io_ctx, logger, http_conf,
              http::https_connection::load_server_certificate, weak);
        });
  else
    _server = http::server::load(
        _io_context, http_logger, http_conf,
        [http_conf, io_ctx = _io_context, logger = http_logger, weak]() {
          return std::make_shared<session<http::http_connection>>(
              io_ctx, logger, http_conf, nullptr, weak);
        });

  _housekeeping = std::make_unique<asio::steady_timer>(*_io_context);
  _start_housekeeping();
  SPDLOG_LOGGER_INFO(_logger, "remote api: listening on {}:{} ({})",
                     _conf.listen_address, _conf.port,
                     crypted ? "https" : "http");
}

/**
 * @brief Stop the server, fail every pending request.
 */
void api::stop() {
  std::vector<std::shared_ptr<command_request>> to_finish;
  {
    absl::MutexLock lck(&_m);
    _stopped = true;
    for (auto& p : _commands)
      to_finish.push_back(p.second.first);
  }
  if (_server)
    _server->shutdown();
  if (_housekeeping)
    _housekeeping->cancel();
  absl::MutexLock lck(&_m);
  for (auto& req : to_finish)
    _finish_command(req);
}

http::response_ptr api::_response(boost::beast::http::status code,
                                  const json& body) {
  auto resp = std::make_shared<http::response_type>();
  resp->result(code);
  resp->insert(boost::beast::http::field::content_type, "application/json");
  resp->body() = body.dump();
  resp->content_length(resp->body().length());
  return resp;
}

http::response_ptr api::_error(boost::beast::http::status code,
                               const std::string& status,
                               const std::string& message) {
  return _response(code, json{{"status", status}, {"message", message}});
}

/**
 * @brief Entry point of every HTTP request: authentication, routing.
 */
void api::handle(const std::shared_ptr<http::request_type>& request,
                 answer_function&& answer) {
  namespace bh = boost::beast::http;
  try {
    auto auth = request->find(bh::field::authorization);
    std::string expected = "Bearer " + _conf.token;
    if (auth == request->end() ||
        !same_secret(std::string(auth->value()), expected)) {
      SPDLOG_LOGGER_WARN(_logger, "remote api: unauthorized request on {}",
                         std::string(request->target()));
      answer(_error(bh::status::unauthorized, "UNAUTHORIZED",
                    "missing or wrong bearer token"));
      return;
    }

    std::string target(request->target());
    auto q = target.find('?');
    if (q != std::string::npos)
      target.resize(q);

    if (request->method() == bh::verb::get && target == "/v1/pollers") {
      answer(_response(bh::status::ok, _pollers_json()));
      return;
    }

    if (request->method() == bh::verb::get && target == "/v1/ca") {
      json ca;
      if (_ca_json(ca))
        answer(_response(bh::status::ok, ca));
      else
        answer(_error(bh::status::not_found, "CA_UNAVAILABLE",
                      fmt::format("no CA certificate in '{}'",
                                  _conf.ca_certificate)));
      return;
    }

    if (request->method() == bh::verb::get &&
        absl::StartsWith(target, "/v1/deployments/")) {
      std::string id = target.substr(strlen("/v1/deployments/"));
      absl::MutexLock lck(&_m);
      auto it = _deployments.find(id);
      if (it == _deployments.end())
        answer(_error(bh::status::not_found, "NOT_FOUND",
                      fmt::format("unknown deployment '{}'", id)));
      else
        answer(_response(bh::status::ok, _deployment_json(*it->second)));
      return;
    }

    if (request->method() == bh::verb::get &&
        absl::StartsWith(target, "/v1/executions/")) {
      std::string id = target.substr(strlen("/v1/executions/"));
      absl::MutexLock lck(&_m);
      auto it = _executions.find(id);
      if (it == _executions.end())
        answer(_error(bh::status::not_found, "NOT_FOUND",
                      fmt::format("unknown execution '{}'", id)));
      else
        answer(_response(bh::status::ok, _execution_json(*it->second)));
      return;
    }

    if (request->method() == bh::verb::get &&
        absl::StartsWith(target, "/v1/plugin-installs/")) {
      std::string id = target.substr(strlen("/v1/plugin-installs/"));
      absl::MutexLock lck(&_m);
      auto it = _installs.find(id);
      if (it == _installs.end())
        answer(_error(bh::status::not_found, "NOT_FOUND",
                      fmt::format("unknown plugin install '{}'", id)));
      else
        answer(_response(bh::status::ok, _install_json(*it->second)));
      return;
    }

    if (request->method() == bh::verb::post &&
        target == "/v1/plugin-installs") {
      json body = json::parse(request->body(), nullptr, false);
      if (body.is_discarded() || !body.is_object()) {
        answer(_error(bh::status::bad_request, "BAD_REQUEST",
                      "body is not a JSON object"));
        return;
      }
      _post_plugin_install(body, std::move(answer));
      return;
    }

    if (request->method() == bh::verb::post && target == "/v1/executions") {
      json body = json::parse(request->body(), nullptr, false);
      if (body.is_discarded() || !body.is_object()) {
        answer(_error(bh::status::bad_request, "BAD_REQUEST",
                      "body is not a JSON object"));
        return;
      }
      _post_execution(body, std::move(answer));
      return;
    }

    if (request->method() == bh::verb::post &&
        (target == "/v1/commands" || target == "/v1/deployments")) {
      json body = json::parse(request->body(), nullptr, false);
      if (body.is_discarded() || !body.is_object()) {
        answer(_error(bh::status::bad_request, "BAD_REQUEST",
                      "body is not a JSON object"));
        return;
      }
      if (target == "/v1/commands")
        _post_commands(body, std::move(answer));
      else
        _post_deployment(body, std::move(answer));
      return;
    }

    answer(_error(bh::status::not_found, "NOT_FOUND",
                  fmt::format("no route for {} {}",
                              std::string(request->method_string()), target)));
  } catch (const std::exception& e) {
    SPDLOG_LOGGER_ERROR(_logger, "remote api: internal error: {}", e.what());
    answer(_error(bh::status::internal_server_error, "INTERNAL", e.what()));
  }
}

/**
 * @brief POST /v1/commands: send the commands to each poller and answer
 * when every poller answered, or at the timeout.
 */
void api::_post_commands(const json& body, answer_function&& answer) {
  namespace bh = boost::beast::http;
  auto req = std::make_shared<command_request>();
  req->id = body.contains("request_id") && body["request_id"].is_string()
                ? body["request_id"].get<std::string>()
                : misc::Uuid().to_string();
  int timeout_ms = default_command_timeout_ms;
  if (body.contains("timeout_ms") && body["timeout_ms"].is_number_integer())
    timeout_ms = std::clamp(body["timeout_ms"].get<int>(), 1000,
                            max_command_timeout_ms);
  if (!body.contains("items") || !body["items"].is_array() ||
      body["items"].empty()) {
    answer(_error(bh::status::bad_request, "BAD_REQUEST",
                  "'items' must be a non empty array"));
    return;
  }

  /* Validate everything before sending anything. */
  std::vector<std::vector<std::string>> commands;
  for (const json& item : body["items"]) {
    command_item ci;
    if (!item.is_object() || !item.contains("poller_id") ||
        !read_poller_id(item["poller_id"], ci.poller_id)) {
      answer(_error(bh::status::bad_request, "BAD_REQUEST",
                    "each item needs a valid 'poller_id'"));
      return;
    }
    if (!item.contains("commands") || !item["commands"].is_array() ||
        item["commands"].empty()) {
      answer(_error(bh::status::bad_request, "BAD_REQUEST",
                    fmt::format("poller {}: 'commands' must be a non empty "
                                "array",
                                ci.poller_id)));
      return;
    }
    std::vector<std::string> cmds;
    for (const json& c : item["commands"]) {
      if (!c.is_string() || c.get<std::string>().empty() ||
          c.get<std::string>().find('\n') != std::string::npos) {
        answer(_error(bh::status::bad_request, "BAD_REQUEST",
                      fmt::format("poller {}: each command must be a one line "
                                  "string",
                                  ci.poller_id)));
        return;
      }
      cmds.push_back(c.get<std::string>());
    }
    req->items.push_back(std::move(ci));
    commands.push_back(std::move(cmds));
  }

  time_t deadline = time(nullptr) + (timeout_ms + 999) / 1000;
  absl::MutexLock lck(&_m);
  req->answer = std::move(answer);
  for (size_t i = 0; i < req->items.size(); ++i) {
    command_item& ci = req->items[i];
    auto channel =
        bbdo::remote_registry::instance().find(ci.poller_id,
                                                      com::centreon::common::ENGINE);
    if (!channel) {
      ci.status = st_not_connected;
      ci.message = "no BBDO stream from this poller engine";
      continue;
    }
    if (!channel->info().remote_actions) {
      ci.status = st_peer_too_old;
      ci.message = "poller does not accept remote actions";
      continue;
    }
    std::string wire_id = fmt::format("{}#{}", req->id, i);
    auto ev = std::make_shared<bbdo::pb_remote_command>();
    auto& obj = ev->mut_obj();
    obj.set_request_id(wire_id);
    obj.set_deadline(deadline);
    size_t size = 0;
    for (auto& c : commands[i]) {
      size += c.size();
      obj.add_commands(c);
    }
    if (!channel->push(ev, size)) {
      ci.status = st_not_connected;
      ci.message = "poller stream closed or queue full";
      continue;
    }
    ci.results.assign(commands[i].size(),
                      {st_timeout, "no answer before the timeout"});
    ci.pending = commands[i].size();
    _commands[wire_id] = {req, i};
    SPDLOG_LOGGER_DEBUG(_logger,
                        "remote api: request {} sends {} commands to poller {}",
                        wire_id, commands[i].size(), ci.poller_id);
  }

  bool pending = false;
  for (auto& ci : req->items)
    if (ci.pending)
      pending = true;
  if (!pending) {
    _finish_command(req);
    return;
  }
  req->timer = std::make_unique<asio::steady_timer>(*_io_context);
  req->timer->expires_after(std::chrono::milliseconds(timeout_ms));
  std::weak_ptr<api> weak = shared_from_this();
  req->timer->async_wait([weak, req](const boost::system::error_code& err) {
    if (err)
      return;
    auto a = weak.lock();
    if (!a)
      return;
    absl::MutexLock lck(&a->_m);
    a->_finish_command(req);
  });
}

/**
 * @brief Compute the item statuses, answer once, forget the request.
 */
void api::_finish_command(const std::shared_ptr<command_request>& req) {
  if (req->done)
    return;
  req->done = true;
  if (req->timer)
    req->timer->cancel();
  for (size_t i = 0; i < req->items.size(); ++i)
    _commands.erase(fmt::format("{}#{}", req->id, i));
  for (auto& ci : req->items) {
    if (!ci.status.empty())
      continue;
    std::string worst = st_applied;
    for (auto& r : ci.results)
      if (status_weight(r.first) > status_weight(worst))
        worst = r.first;
    ci.status = worst;
  }
  for (auto& ci : req->items)
    SPDLOG_LOGGER_INFO(_logger,
                       "remote api: commands request {} poller {}: {} {}",
                       req->id, ci.poller_id, ci.status, ci.message);
  json j = _command_json(*req);
  auto answer = std::move(req->answer);
  if (answer)
    asio::post(*_io_context, [answer, j]() {
      answer(_response(boost::beast::http::status::ok, j));
    });
}

json api::_command_json(const command_request& req) const {
  json items = json::array();
  for (auto& ci : req.items) {
    json results = json::array();
    for (size_t i = 0; i < ci.results.size(); ++i)
      results.push_back({{"index", i},
                         {"status", ci.results[i].first},
                         {"message", ci.results[i].second}});
    json item{{"poller_id", std::to_string(ci.poller_id)},
              {"status", ci.status},
              {"message", ci.message}};
    if (!ci.results.empty())
      item["results"] = results;
    items.push_back(item);
  }
  return json{{"request_id", req.id}, {"items", items}};
}

/**
 * @brief POST /v1/deployments: queue the files as chunks then a commit on
 * the poller stream. The status is polled with GET /v1/deployments/{id}.
 */
void api::_post_deployment(const json& body, answer_function&& answer) {
  namespace bh = boost::beast::http;
  auto d = std::make_shared<deployment>();
  d->id = body.contains("request_id") && body["request_id"].is_string()
              ? body["request_id"].get<std::string>()
              : misc::Uuid().to_string();
  if (d->id.empty() || d->id.find('/') != std::string::npos ||
      d->id.find('#') != std::string::npos) {
    answer(_error(bh::status::bad_request, "BAD_REQUEST",
                  "'request_id' must not contain '/' or '#'"));
    return;
  }
  if (!body.contains("poller_id") ||
      !read_poller_id(body["poller_id"], d->poller_id)) {
    answer(_error(bh::status::bad_request, "BAD_REQUEST",
                  "'poller_id' is missing or invalid"));
    return;
  }
  d->action = body.contains("action") && body["action"].is_string()
                  ? body["action"].get<std::string>()
                  : "reload";
  if (d->action != "none" && d->action != "reload" && d->action != "restart") {
    answer(_error(bh::status::bad_request, "BAD_REQUEST",
                  "'action' must be none, reload or restart"));
    return;
  }

  /* Decode and validate every file first. */
  struct file {
    std::string target, path, sha256, content;
  };
  std::vector<file> files;
  if (body.contains("files")) {
    if (!body["files"].is_array()) {
      answer(_error(bh::status::bad_request, "BAD_REQUEST",
                    "'files' must be an array"));
      return;
    }
    for (const json& f : body["files"]) {
      file fl;
      if (!f.is_object() || !f.contains("target") || !f["target"].is_string() ||
          !f.contains("path") || !f["path"].is_string() ||
          !f.contains("sha256") || !f["sha256"].is_string() ||
          !f.contains("content_b64") || !f["content_b64"].is_string()) {
        answer(_error(bh::status::bad_request, "BAD_REQUEST",
                      "each file needs target, path, sha256, content_b64"));
        return;
      }
      fl.target = f["target"].get<std::string>();
      fl.path = f["path"].get<std::string>();
      fl.sha256 = absl::AsciiStrToLower(f["sha256"].get<std::string>());
      if (fl.target != "engine" && fl.target != "broker") {
        answer(_error(bh::status::bad_request, "BAD_REQUEST",
                      fmt::format("file '{}': target must be engine or broker",
                                  fl.path)));
        return;
      }
      if (!valid_relative_path(fl.path) || !valid_sha256(fl.sha256)) {
        answer(_error(bh::status::bad_request, "BAD_REQUEST",
                      fmt::format("file '{}': invalid path or sha256",
                                  fl.path)));
        return;
      }
      if (!absl::Base64Unescape(f["content_b64"].get<std::string>(),
                                &fl.content)) {
        answer(_error(bh::status::bad_request, "BAD_REQUEST",
                      fmt::format("file '{}': content_b64 is not base64",
                                  fl.path)));
        return;
      }
      if (fl.target == "broker")
        d->broker_files = true;
      files.push_back(std::move(fl));
    }
  }

  auto channel =
      bbdo::remote_registry::instance().find(d->poller_id,
                                               com::centreon::common::ENGINE);
  if (!channel) {
    d->status = st_not_connected;
    d->message = "no BBDO stream from this poller engine";
  } else if (!channel->info().remote_actions) {
    d->status = st_peer_too_old;
    d->message = "poller does not accept remote actions";
  } else {
    d->own_poller =
        d->poller_id == config::applier::state::instance().poller_id() ||
        (_conf.local_poller_uid != 0 &&
         d->poller_id == _conf.local_poller_uid);
    bool ok = true;
    for (uint32_t idx = 0; ok && idx < files.size(); ++idx) {
      const file& fl = files[idx];
      size_t offset = 0;
      do {
        auto ev = std::make_shared<bbdo::pb_config_chunk>();
        auto& obj = ev->mut_obj();
        obj.set_request_id(d->id);
        obj.set_index(idx);
        obj.set_target(fl.target);
        obj.set_path(fl.path);
        obj.set_sha256(fl.sha256);
        size_t len = std::min(chunk_size, fl.content.size() - offset);
        obj.set_data(fl.content.substr(offset, len));
        offset += len;
        obj.set_last(offset >= fl.content.size());
        ok = channel->push(ev, len + fl.path.size());
      } while (ok && offset < fl.content.size());
    }
    if (ok) {
      auto commit = std::make_shared<bbdo::pb_config_commit>();
      commit->mut_obj().set_request_id(d->id);
      commit->mut_obj().set_action(d->action);
      commit->mut_obj().set_files_count(files.size());
      ok = channel->push(commit, 0);
    }
    if (ok) {
      d->status = st_transferring;
    } else {
      d->status = st_rejected;
      d->message = "poller stream closed or queue full";
    }
  }
  d->last_progress = time(nullptr);
  if (is_final(d->status))
    d->finished_at = d->last_progress;
  SPDLOG_LOGGER_INFO(_logger,
                     "remote api: deployment {} of {} files to poller {}: {}",
                     d->id, files.size(), d->poller_id, d->status);

  json j;
  {
    absl::MutexLock lck(&_m);
    _deployments[d->id] = d;
    j = _deployment_json(*d);
  }
  if (d->status == st_transferring) {
    j["status"] = st_accepted;
    answer(_response(bh::status::accepted, j));
  } else {
    answer(_response(bh::status::ok, j));
  }
}

/**
 * @brief POST /v1/executions: send a RemoteExec to one poller engine. The
 * answer is 202 ACCEPTED, or 200 with a final status when nothing was sent.
 * argv is validated, pushed and forgotten: never stored nor logged.
 */
void api::_post_execution(const json& body, answer_function&& answer) {
  namespace bh = boost::beast::http;
  auto bad = [&answer](const std::string& msg) {
    answer(_error(bh::status::bad_request, "BAD_REQUEST", msg));
  };
  auto e = std::make_shared<execution>();
  e->id = body.contains("request_id") && body["request_id"].is_string() &&
                  !body["request_id"].get<std::string>().empty()
              ? body["request_id"].get<std::string>()
              : misc::Uuid().to_string();
  if (!body.contains("poller_id") ||
      !read_poller_id(body["poller_id"], e->poller_id)) {
    bad("'poller_id' must be a positive integer");
    return;
  }
  if (!body.contains("argv") || !body["argv"].is_array() ||
      body["argv"].empty() || body["argv"].size() > exec_max_args) {
    bad(fmt::format("'argv' must be an array of 1 to {} strings",
                    exec_max_args));
    return;
  }
  std::vector<std::string> argv;
  size_t argv_bytes = 0;
  for (const json& a : body["argv"]) {
    if (!a.is_string()) {
      bad("'argv' must contain strings only");
      return;
    }
    argv.push_back(a.get<std::string>());
    argv_bytes += argv.back().size();
  }
  if (argv_bytes > exec_max_argv_bytes) {
    bad(fmt::format("'argv' is larger than {} bytes", exec_max_argv_bytes));
    return;
  }
  if (argv[0].empty() || argv[0][0] != '/') {
    bad("argv[0] must be an absolute path");
    return;
  }
  e->program = std::filesystem::path(argv[0]).filename().string();
  if (body.contains("timeout_s")) {
    if (!body["timeout_s"].is_number_unsigned() ||
        body["timeout_s"].get<uint64_t>() == 0 ||
        body["timeout_s"].get<uint64_t>() > exec_max_timeout_s) {
      bad(fmt::format("'timeout_s' must be in 1..{}", exec_max_timeout_s));
      return;
    }
    e->timeout_s = body["timeout_s"].get<uint32_t>();
  }
  uint32_t max_output = 0;
  if (body.contains("max_output_bytes")) {
    if (!body["max_output_bytes"].is_number_unsigned() ||
        body["max_output_bytes"].get<uint64_t>() > exec_max_output) {
      bad(fmt::format("'max_output_bytes' must be in 0..{}", exec_max_output));
      return;
    }
    max_output = body["max_output_bytes"].get<uint32_t>();
  }

  {
    absl::MutexLock lck(&_m);
    auto it = _executions.find(e->id);
    if (it != _executions.end()) {
      answer(_response(bh::status::ok, _execution_json(*it->second)));
      return;
    }
    size_t pending = 0;
    for (auto& p : _executions)
      if (!exec_is_final(p.second->status))
        ++pending;
    if (pending >= exec_max_pending) {
      answer(_error(bh::status::service_unavailable, "BUSY",
                    fmt::format("{} executions are not finished", pending)));
      return;
    }
  }

  time_t now = time(nullptr);
  e->accepted_at = e->last_progress = now;
  auto channel = bbdo::remote_registry::instance().find(
      e->poller_id, com::centreon::common::ENGINE);
  if (!channel) {
    e->status = st_not_connected;
    e->message = "no BBDO stream from this poller engine";
  } else if (!channel->info().remote_actions ||
             !channel->info().remote_exec) {
    e->status = st_peer_too_old;
    e->message = "poller does not accept remote executions";
  } else if (!channel->info().tls && !_conf.exec_insecure) {
    e->status = st_rejected;
    e->message = "stream not encrypted";
  } else {
    auto ev = std::make_shared<bbdo::pb_remote_exec>();
    auto& obj = ev->mut_obj();
    obj.set_request_id(e->id);
    obj.set_deadline(now + exec_deadline_margin);
    for (auto& a : argv)
      obj.add_argv(std::move(a));
    obj.set_timeout_s(e->timeout_s);
    obj.set_max_output_bytes(max_output);
    if (channel->push(ev, argv_bytes + 64)) {
      e->status = st_accepted;
    } else {
      e->status = st_rejected;
      e->message = "poller stream closed or queue full";
    }
  }
  if (exec_is_final(e->status))
    e->finished_at = now;
  SPDLOG_LOGGER_INFO(_logger, "remote api: execution {} of {} on poller {}: {}",
                     e->id, e->program, e->poller_id, e->status);
  json j;
  {
    absl::MutexLock lck(&_m);
    _executions[e->id] = e;
    j = _execution_json(*e);
  }
  answer(_response(
      e->status == st_accepted ? bh::status::accepted : bh::status::ok, j));
}

json api::_execution_json(const execution& e) const {
  bool is_done = exec_is_final(e.status);
  json j{{"request_id", e.id},
         {"poller_id", std::to_string(e.poller_id)},
         {"status", e.status},
         {"message", e.message},
         {"exit_code", nullptr},
         {"output_b64", is_done ? absl::Base64Escape(e.output) : ""},
         {"output_bytes", e.output_bytes},
         {"truncated", e.truncated},
         {"stderr_tail", e.stderr_tail},
         {"accepted_at", e.accepted_at},
         {"last_progress", e.last_progress},
         {"finished_at", e.finished_at}};
  if (e.has_exit_code)
    j["exit_code"] = e.exit_code;
  return j;
}

void api::_finish_execution(execution& e,
                            const char* status,
                            const std::string& message,
                            time_t now) {
  e.status = status;
  e.message = message;
  e.finished_at = now;
  SPDLOG_LOGGER_INFO(_logger,
                     "remote api: execution {} of {} on poller {}: {} {}, {} "
                     "bytes{}",
                     e.id, e.program, e.poller_id, e.status, e.message,
                     e.output_bytes, e.truncated ? ", truncated" : "");
}

/**
 * @brief A RemoteExecResult arrived from a poller (feeder thread of its
 * stream). Output chunks are appended in sequence order.
 */
void api::_on_exec_result(uint64_t poller_id, const RemoteExecResult& r) {
  time_t now = time(nullptr);
  absl::MutexLock lck(&_m);
  auto it = _executions.find(r.request_id());
  if (it == _executions.end())
    return;
  execution& e = *it->second;
  if (e.poller_id != poller_id || exec_is_final(e.status))
    return;
  e.last_progress = now;
  if (r.seq() != e.last_seq + 1) {
    _finish_execution(e, st_failed, "output lost", now);
    e.output.clear();
    return;
  }
  e.last_seq = r.seq();
  if (!r.stdout_chunk().empty() &&
      e.output.size() + r.stdout_chunk().size() <= exec_max_output)
    e.output.append(r.stdout_chunk());
  switch (r.state()) {
    case EXEC_RUNNING:
      if (e.status != st_running) {
        e.status = st_running;
        e.started_at = now;
      }
      return;
    case EXEC_DONE:
      e.has_exit_code = true;
      e.exit_code = r.exit_code();
      break;
    case EXEC_REJECTED:
    case EXEC_TIMEOUT:
    case EXEC_FAILED:
      break;
    default:
      return;
  }
  e.output_bytes = r.output_bytes();
  e.truncated = r.truncated();
  e.stderr_tail = com::centreon::common::check_string_utf8(r.stderr_tail());
  _finish_execution(e,
                    r.state() == EXEC_DONE       ? st_done
                    : r.state() == EXEC_REJECTED ? st_rejected
                    : r.state() == EXEC_TIMEOUT  ? st_timeout
                                                 : st_failed,
                    com::centreon::common::check_string_utf8(r.message()), now);
}

json api::_deployment_json(const deployment& d) const {
  json j{{"request_id", d.id},
         {"poller_id", std::to_string(d.poller_id)},
         {"status", d.status},
         {"message", d.message},
         {"config_version", d.config_version},
         {"plugins_missing", d.plugins_missing}};
  if (!d.plugins.is_null())
    j["plugins"] = d.plugins;
  return j;
}

/**
 * @brief GET /v1/ca: the first certificate of the CA file (public material
 * only) and the SHA-256 of its DER encoding, as lowercase hex.
 *
 * @return false if the file is unreadable or holds no certificate.
 */
bool api::_ca_json(json& out) const {
  constexpr std::string_view begin = "-----BEGIN CERTIFICATE-----";
  constexpr std::string_view end = "-----END CERTIFICATE-----";
  std::ifstream f(_conf.ca_certificate);
  if (!f)
    return false;
  std::stringstream ss;
  ss << f.rdbuf();
  const std::string content = ss.str();
  size_t b = content.find(begin);
  if (b == std::string::npos)
    return false;
  size_t e = content.find(end, b + begin.size());
  if (e == std::string::npos)
    return false;
  std::string body;
  for (size_t i = b + begin.size(); i < e; ++i)
    if (!absl::ascii_isspace(static_cast<unsigned char>(content[i])))
      body.push_back(content[i]);
  std::string der;
  if (!absl::Base64Unescape(body, &der) || der.empty())
    return false;
  unsigned char hash[EVP_MAX_MD_SIZE];
  unsigned int size = 0;
  if (!EVP_Digest(der.data(), der.size(), hash, &size, EVP_sha256(), nullptr))
    return false;
  out = json{{"pem", content.substr(b, e + end.size() - b) + "\n"},
             {"fingerprint_sha256",
              absl::BytesToHexString(std::string_view(
                  reinterpret_cast<const char*>(hash), size))}};
  return true;
}

/**
 * @brief POST /v1/plugin-installs: send a PluginInstall to one poller engine.
 * cbd checks the shape only: the poller is the single judge of the package
 * names (contracts.md §14.2), so a forged name comes back REJECTED from it.
 */
void api::_post_plugin_install(const json& body, answer_function&& answer) {
  namespace bh = boost::beast::http;
  auto bad = [&answer](const std::string& msg) {
    answer(_error(bh::status::bad_request, "BAD_REQUEST", msg));
  };
  auto i = std::make_shared<plugin_install>();
  if (body.contains("request_id")) {
    if (!body["request_id"].is_string() ||
        body["request_id"].get<std::string>().size() > install_max_id) {
      bad(fmt::format("'request_id' must be a string of at most {} characters",
                      install_max_id));
      return;
    }
    i->id = body["request_id"].get<std::string>();
  }
  if (i->id.empty())
    i->id = misc::Uuid().to_string();
  if (!body.contains("poller_id") ||
      !read_poller_id(body["poller_id"], i->poller_id)) {
    bad("'poller_id' must be a positive integer");
    return;
  }
  if (!body.contains("packages") || !body["packages"].is_object() ||
      body["packages"].empty() ||
      body["packages"].size() > install_max_packages) {
    bad(fmt::format("'packages' must be an object of 1 to {} entries",
                    install_max_packages));
    return;
  }
  std::map<std::string, std::string> packages;
  for (auto& [name, version] : body["packages"].items()) {
    if (!version.is_string()) {
      bad("'packages' values must be strings");
      return;
    }
    if (name.size() > install_max_field ||
        version.get<std::string>().size() > install_max_field) {
      bad(fmt::format("package names and versions are limited to {} bytes",
                      install_max_field));
      return;
    }
    packages.emplace(name, version.get<std::string>());
  }
  if (body.contains("timeout_s")) {
    if (!body["timeout_s"].is_number_unsigned() ||
        body["timeout_s"].get<uint64_t>() > install_max_timeout_s) {
      bad(fmt::format("'timeout_s' must be in 0..{}", install_max_timeout_s));
      return;
    }
    uint32_t t = body["timeout_s"].get<uint32_t>();
    if (t)
      i->timeout_s = t;
  }
  {
    std::vector<std::string> names;
    for (auto& p : packages)
      names.push_back(com::centreon::common::check_string_utf8(p.first));
    i->names = absl::StrJoin(names, " ");
  }

  {
    absl::MutexLock lck(&_m);
    auto it = _installs.find(i->id);
    if (it != _installs.end()) {
      answer(_response(bh::status::ok, _install_json(*it->second)));
      return;
    }
    size_t pending = 0;
    for (auto& p : _installs)
      if (!install_is_final(p.second->status))
        ++pending;
    if (pending >= install_max_pending) {
      answer(_error(bh::status::service_unavailable, "BUSY",
                    fmt::format("{} plugin installs are not finished",
                                pending)));
      return;
    }
  }

  time_t now = time(nullptr);
  i->accepted_at = i->updated_at = now;
  auto channel = bbdo::remote_registry::instance().find(
      i->poller_id, com::centreon::common::ENGINE);
  if (!channel) {
    i->status = st_not_connected;
    i->message = "no BBDO stream from this poller engine";
  } else if (!channel->info().remote_actions ||
             !channel->info().plugin_install) {
    i->status = st_peer_too_old;
    i->message = "poller does not accept plugin installs";
  } else if (!channel->info().tls && !_conf.exec_insecure) {
    i->status = st_rejected;
    i->message = "stream not encrypted";
  } else {
    auto ev = std::make_shared<bbdo::pb_plugin_install>();
    auto& obj = ev->mut_obj();
    obj.set_request_id(i->id);
    obj.set_deadline(now + install_deadline_margin);
    obj.set_timeout_s(i->timeout_s);
    size_t bytes = 64;
    for (auto& [name, version] : packages) {
      bytes += name.size() + version.size();
      (*obj.mutable_packages())[name] = version;
    }
    if (channel->push(ev, bytes)) {
      i->status = st_accepted;
    } else {
      i->status = st_rejected;
      i->message = "poller stream closed or queue full";
    }
  }
  if (install_is_final(i->status))
    i->finished_at = now;
  SPDLOG_LOGGER_INFO(_logger,
                     "remote api: plugin install {} of {} on poller {}: {} {}",
                     i->id, i->names, i->poller_id, i->status, i->message);
  json j;
  {
    absl::MutexLock lck(&_m);
    _installs[i->id] = i;
    j = _install_json(*i);
  }
  answer(_response(
      i->status == st_accepted ? bh::status::accepted : bh::status::ok, j));
}

json api::_install_json(const plugin_install& i) const {
  return json{{"request_id", i.id},
              {"poller_id", std::to_string(i.poller_id)},
              {"status", i.status},
              {"message", i.message},
              {"packages", i.packages},
              {"accepted_at", i.accepted_at},
              {"updated_at", i.updated_at},
              {"finished_at", i.finished_at}};
}

void api::_finish_install(plugin_install& i,
                          const char* status,
                          const std::string& message,
                          time_t now) {
  i.status = status;
  i.message = message;
  i.updated_at = i.finished_at = now;
  SPDLOG_LOGGER_INFO(_logger,
                     "remote api: plugin install {} of {} on poller {}: {} {}",
                     i.id, i.names, i.poller_id, i.status, i.message);
}

/**
 * @brief A RemoteResult that answers a plugin install request: RUNNING, then
 * INSTALLED, PARTIAL or FAILED from the packages, or REJECTED / TIMEOUT.
 *
 * @return true if the result belongs to a plugin install request.
 */
bool api::_on_install_result(uint64_t poller_id, const RemoteResult& r) {
  auto it = _installs.find(r.request_id());
  if (it == _installs.end())
    return false;
  plugin_install& i = *it->second;
  if (i.poller_id != poller_id || install_is_final(i.status))
    return true;
  time_t now = time(nullptr);
  i.updated_at = now;
  switch (r.status()) {
    case REMOTE_DELIVERED:
      if (i.status == st_accepted) {
        i.status = st_running;
        i.started_at = now;
      }
      break;
    case REMOTE_APPLIED: {
      size_t installed = 0, failed = 0;
      json packages = json::array();
      for (auto& p : r.plugins()) {
        bool ok = p.status() == "installed";
        if (ok)
          ++installed;
        else
          ++failed;
        packages.push_back({{"name", p.name()},
                            {"status", ok ? "installed" : "failed"},
                            {"message", p.message()}});
      }
      i.packages = std::move(packages);
      _finish_install(i,
                      failed == 0      ? st_installed
                      : installed == 0 ? st_failed
                                       : st_partial,
                      r.message(), now);
    } break;
    case REMOTE_REJECTED:
      _finish_install(i, st_rejected, r.message(), now);
      break;
    case REMOTE_TIMEOUT:
      _finish_install(i, st_timeout, r.message(), now);
      break;
    default:
      break;
  }
  return true;
}

json api::_pollers_json() const {
  json pollers = json::array();
  auto channels = bbdo::remote_registry::instance().list();
  absl::MutexLock lck(&_m);
  for (auto& c : channels) {
    const auto& i = c->info();
    if (i.peer_type != com::centreon::common::ENGINE)
      continue;
    auto v = _config_versions.find(i.poller_id);
    pollers.push_back({{"poller_id", std::to_string(i.poller_id)},
                       {"poller_name", i.poller_name},
                       {"broker_name", i.broker_name},
                       {"peer_type", "ENGINE"},
                       {"remote_actions", i.remote_actions},
                       {"tls", i.tls},
                       {"remote_exec", i.remote_exec},
                       {"plugin_install", i.plugin_install},
                       {"connected_since", i.connected_since},
                       {"config_version",
                        v == _config_versions.end() ? "" : v->second}});
    auto p = _plugins.find(i.poller_id);
    if (p != _plugins.end())
      pollers.back()["plugins"] = p->second;
  }
  return json{{"pollers", pollers}};
}

/**
 * @brief Plugin install outcome (RemoteResult index -2) as exposed by the API:
 * RUNNING while the install runs, then INSTALLED, FAILED or PARTIAL.
 */
static json plugins_json(const RemoteResult& r) {
  std::string status;
  json packages = json::array();
  if (r.status() == REMOTE_DELIVERED) {
    status = "RUNNING";
  } else if (r.status() == REMOTE_REJECTED) {
    status = "FAILED";
  } else {
    size_t installed = 0, failed = 0;
    for (auto& p : r.plugins()) {
      if (p.status() == "installed")
        ++installed;
      else
        ++failed;
      packages.push_back({{"name", p.name()},
                          {"status", p.status() == "installed" ? "installed"
                                                               : "failed"},
                          {"message", p.message()}});
    }
    status = failed == 0 ? "INSTALLED" : installed == 0 ? "FAILED" : "PARTIAL";
  }
  return json{{"deployment_id", r.request_id()},
              {"status", status},
              {"updated_at", time(nullptr)},
              {"message", r.message()},
              {"packages", packages}};
}

/**
 * @brief A RemoteResult arrived from a poller (feeder thread of its stream).
 */
void api::_on_result(uint64_t poller_id, const RemoteResult& r) {
  {
    /* Plugin install requests first: their answers never touch the
     * deployments nor the last plugins.json result of the peer. */
    absl::MutexLock lck(&_m);
    if (_on_install_result(poller_id, r))
      return;
  }
  std::string status;
  switch (r.status()) {
    case REMOTE_DELIVERED:
      status = st_delivered;
      break;
    case REMOTE_APPLIED:
      status = st_applied;
      break;
    case REMOTE_REJECTED:
      status = st_rejected;
      break;
    default:
      return;
  }
  absl::MutexLock lck(&_m);
  if (r.index() == plugins_result_index) {
    /* Plugin install outcome: independent of the deployment status, which
     * may already be final. */
    json p = plugins_json(r);
    SPDLOG_LOGGER_INFO(_logger,
                       "remote api: plugin install of deployment {} on poller "
                       "{}: {} {}",
                       r.request_id(), poller_id, p["status"].get<std::string>(),
                       r.message());
    auto dit = _deployments.find(r.request_id());
    if (dit != _deployments.end() && dit->second->poller_id == poller_id)
      dit->second->plugins = p;
    _plugins[poller_id] = std::move(p);
    return;
  }
  auto cit = _commands.find(r.request_id());
  if (cit != _commands.end()) {
    auto req = cit->second.first;
    command_item& ci = req->items[cit->second.second];
    if (ci.poller_id != poller_id)
      return;
    if (r.index() < 0) {
      /* Refusal of the whole request. */
      for (auto& res : ci.results)
        res = {status, r.message()};
      ci.pending = 0;
    } else if (static_cast<size_t>(r.index()) < ci.results.size() &&
               status != st_delivered) {
      auto& res = ci.results[r.index()];
      if (res.first == st_timeout && ci.pending)
        --ci.pending;
      res = {status, r.message()};
    }
    bool done = true;
    for (auto& i : req->items)
      if (i.pending)
        done = false;
    if (done)
      _finish_command(req);
    return;
  }

  auto dit = _deployments.find(r.request_id());
  if (dit != _deployments.end()) {
    deployment& d = *dit->second;
    if (d.poller_id != poller_id || is_final(d.status))
      return;
    d.last_progress = time(nullptr);
    if (status == st_delivered) {
      d.status = st_delivered;
      return;
    }
    d.status = status;
    d.message = r.message();
    d.config_version = r.config_version();
    d.plugins_missing.assign(r.plugins_missing().begin(),
                             r.plugins_missing().end());
    d.finished_at = d.last_progress;
    if (status == st_applied && !d.config_version.empty())
      _config_versions[poller_id] = d.config_version;
    if (status == st_applied && d.broker_files && d.own_poller) {
      SPDLOG_LOGGER_INFO(_logger,
                         "remote api: deployment {} changed the configuration "
                         "of this broker, reloading it",
                         d.id);
      asio::post(*_io_context, []() { ::kill(::getpid(), SIGHUP); });
    }
    SPDLOG_LOGGER_INFO(_logger, "remote api: deployment {} on poller {}: {} {}",
                       d.id, poller_id, d.status, d.message);
  }
}

/**
 * @brief The engine stream of a poller is gone: what it did not answer fails
 * now with NOT_CONNECTED.
 */
void api::_on_disconnect(uint64_t poller_id) {
  absl::MutexLock lck(&_m);
  std::vector<std::shared_ptr<command_request>> to_finish;
  for (auto& p : _commands) {
    auto& req = p.second.first;
    command_item& ci = req->items[p.second.second];
    if (ci.poller_id != poller_id || !ci.pending)
      continue;
    for (auto& res : ci.results)
      if (res.first == st_timeout)
        res = {st_not_connected, "poller disconnected"};
    ci.pending = 0;
    bool done = true;
    for (auto& i : req->items)
      if (i.pending)
        done = false;
    if (done)
      to_finish.push_back(req);
  }
  for (auto& req : to_finish)
    _finish_command(req);

  time_t now = time(nullptr);
  for (auto& p : _deployments) {
    deployment& d = *p.second;
    if (d.poller_id == poller_id && !is_final(d.status)) {
      /* A restart deployment ends with the poller going away. */
      d.status = st_not_connected;
      d.message = "poller disconnected during the deployment";
      d.finished_at = now;
    }
  }
  /* The config version is kept: the delivered files stay on the poller disk
   * and a restart deployment reconnects with them. */

  /* An install still running is lost with the engine (restart): report it. */
  auto pit = _plugins.find(poller_id);
  if (pit != _plugins.end() && pit->second["status"] == "RUNNING") {
    json& p = pit->second;
    p["status"] = "FAILED";
    p["message"] = "interrupted: poller disconnected during the install";
    p["updated_at"] = now;
    auto dit = _deployments.find(p["deployment_id"].get<std::string>());
    if (dit != _deployments.end() && dit->second->poller_id == poller_id)
      dit->second->plugins = p;
  }

  for (auto& p : _installs) {
    plugin_install& i = *p.second;
    if (i.poller_id == poller_id && !install_is_final(i.status))
      _finish_install(i, st_not_connected,
                      "poller disconnected during the install", now);
  }

  /* Results of a run in progress are lost with the stream (no replay). */
  for (auto& p : _executions) {
    execution& e = *p.second;
    if (e.poller_id == poller_id && !exec_is_final(e.status))
      _finish_execution(e, st_not_connected,
                        "poller disconnected during the execution", now);
  }
}

void api::_start_housekeeping() {
  _housekeeping->expires_after(std::chrono::seconds(1));
  std::weak_ptr<api> weak = shared_from_this();
  _housekeeping->async_wait([weak](const boost::system::error_code& err) {
    if (err)
      return;
    auto a = weak.lock();
    if (!a)
      return;
    a->_housekeep();
    a->_start_housekeeping();
  });
}

/**
 * @brief Time out deployments without progress, forget old ones.
 */
void api::_housekeep() {
  time_t now = time(nullptr);
  absl::MutexLock lck(&_m);
  for (auto it = _deployments.begin(); it != _deployments.end();) {
    deployment& d = *it->second;
    if (!is_final(d.status) &&
        now - d.last_progress > deployment_progress_timeout) {
      d.status = st_timeout;
      d.message = fmt::format("no progress from the poller for {}s",
                              deployment_progress_timeout);
      d.finished_at = now;
    }
    if (d.finished_at && now - d.finished_at > deployment_keep_duration)
      _deployments.erase(it++);
    else
      ++it;
  }

  /* Plugin installs: never started (queued behind another install too
   * long), or no result past their timeout. */
  for (auto it = _installs.begin(); it != _installs.end();) {
    plugin_install& i = *it->second;
    if (i.status == st_accepted && now - i.accepted_at > install_start_timeout)
      _finish_install(i, st_timeout, "not started by the poller", now);
    else if (i.status == st_running &&
             now - i.started_at >
                 static_cast<time_t>(i.timeout_s) + install_deadline_margin)
      _finish_install(i, st_timeout, "no result from the poller", now);
    if (i.finished_at && now - i.finished_at > install_keep_duration)
      _installs.erase(it++);
    else
      ++it;
  }

  /* Executions: no progress (3 missed heartbeats), never started by the
   * poller (queued too long), or past their deadline. */
  size_t kept = 0;
  std::vector<std::shared_ptr<execution>> finals;
  for (auto it = _executions.begin(); it != _executions.end();) {
    execution& e = *it->second;
    if (!exec_is_final(e.status)) {
      if (e.status == st_accepted && now - e.accepted_at > exec_start_timeout)
        _finish_execution(e, st_timeout, "not started by the poller", now);
      else if (e.status == st_running &&
               (now - e.last_progress > exec_progress_timeout ||
                now - e.started_at >
                    static_cast<time_t>(e.timeout_s) + exec_deadline_margin))
        _finish_execution(e, st_timeout, "no progress from the poller", now);
    }
    if (e.finished_at && now - e.finished_at > exec_keep_duration) {
      _executions.erase(it++);
      continue;
    }
    if (e.finished_at) {
      kept += e.output.size();
      finals.push_back(it->second);
    }
    ++it;
  }
  /* Bounded output memory: forget the oldest final results first. */
  if (kept > exec_max_kept_output) {
    std::sort(finals.begin(), finals.end(),
              [](const auto& a, const auto& b) {
                return a->finished_at < b->finished_at;
              });
    for (auto& e : finals) {
      if (kept <= exec_max_kept_output)
        break;
      kept -= e->output.size();
      _executions.erase(e->id);
    }
  }
}

}  // namespace

/**
 * @brief Start the API if the configuration has a `remote_api` object.
 */
bool remote_api::load(const std::map<std::string, std::string>& params) {
  auto logger = log_v2::instance().get(log_v2::CORE);
  auto it = params.find("remote_api");
  if (it == params.end())
    return false;
  try {
    json j = json::parse(it->second);
    api::conf c;
    auto get = [&j](const char* key) -> std::string {
      if (!j.contains(key))
        return {};
      if (j[key].is_string())
        return j[key].get<std::string>();
      return j[key].dump();
    };
    if (!get("listen_address").empty())
      c.listen_address = get("listen_address");
    if (!get("port").empty()) {
      uint32_t port;
      if (!absl::SimpleAtoi(get("port"), &port) || port == 0 || port > 65535)
        throw std::invalid_argument("remote_api: invalid port");
      c.port = port;
    }
    c.token = get("token");
    if (absl::StartsWith(c.token, "encrypt::")) {
      if (!credentials_decrypt)
        throw std::invalid_argument(
            "remote_api: encrypted token but no decryption key "
            "(engine-context.json)");
      c.token =
          credentials_decrypt->decrypt(std::string_view(c.token).substr(9));
    }
    if (c.token.empty())
      throw std::invalid_argument("remote_api: a token is mandatory");
    c.public_cert = get("public_cert");
    c.private_key = get("private_key");
    if (!get("ca_certificate").empty())
      c.ca_certificate = get("ca_certificate");
    if (!get("max_body_size").empty() &&
        !absl::SimpleAtoi(get("max_body_size"), &c.max_body_size))
      throw std::invalid_argument("remote_api: invalid max_body_size");
    c.exec_insecure = get("exec_insecure") == "yes" ||
                      get("exec_insecure") == "true";
    if (c.exec_insecure)
      SPDLOG_LOGGER_WARN(logger,
                         "remote api: exec_insecure is set, executions are "
                         "sent on streams without a verified TLS peer (tests "
                         "only)");
    /* Not a JSON key: the configuration delivered by the web has no uid, it
     * would be lost at the first delivery. */
    if (const char* uid = std::getenv("CENTRAL_POLLER_UID"); uid && *uid) {
      if (absl::SimpleAtoi(uid, &c.local_poller_uid) &&
          c.local_poller_uid != 0 &&
          c.local_poller_uid <= static_cast<uint64_t>(INT64_MAX))
        SPDLOG_LOGGER_INFO(logger,
                           "remote api: local engine uid {} (self-reload on "
                           "its deployments)",
                           c.local_poller_uid);
      else {
        c.local_poller_uid = 0;
        SPDLOG_LOGGER_ERROR(logger,
                            "remote api: CENTRAL_POLLER_UID '{}' is not a "
                            "decimal integer in 1..2^63-1, ignored",
                            uid);
      }
    } else {
      SPDLOG_LOGGER_INFO(logger,
                         "remote api: CENTRAL_POLLER_UID not set, self-reload "
                         "only on deployments to poller id {}",
                         config::applier::state::instance().poller_id());
    }

    unload();
    auto a = std::make_shared<api>(c);
    a->start();
    absl::MutexLock lck(&g_api_m);
    g_api = a;
    return true;
  } catch (const std::exception& e) {
    SPDLOG_LOGGER_ERROR(logger, "remote api: not started: {}", e.what());
    return false;
  }
}

/**
 * @brief Stop the API.
 */
void remote_api::unload() {
  std::shared_ptr<api> a;
  {
    absl::MutexLock lck(&g_api_m);
    std::swap(a, g_api);
  }
  if (a)
    a->stop();
}
