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
#include "com/centreon/broker/misc/uuid.hh"
#include "com/centreon/common/http/http_server.hh"
#include "com/centreon/common/http/https_connection.hh"
#include "com/centreon/common/pool.hh"
#include "common/log_v2/log_v2.hh"

#include <absl/strings/ascii.h>
#include <absl/strings/escaping.h>
#include <absl/strings/match.h>
#include <absl/strings/str_split.h>
#include <algorithm>
#include <nlohmann/json.hpp>

using namespace com::centreon::broker;
using com::centreon::common::log_v2::log_v2;
namespace http = com::centreon::common::http;
using json = nlohmann::json;

namespace {

constexpr size_t chunk_size = 1u << 20;
constexpr int default_command_timeout_ms = 10000;
constexpr int max_command_timeout_ms = 30000;
constexpr time_t deployment_progress_timeout = 30;
constexpr time_t deployment_keep_duration = 3600;

/* Status names of the HTTP contract. */
constexpr const char* st_accepted = "ACCEPTED";
constexpr const char* st_not_connected = "NOT_CONNECTED";
constexpr const char* st_peer_too_old = "PEER_TOO_OLD";
constexpr const char* st_transferring = "TRANSFERRING";
constexpr const char* st_delivered = "DELIVERED";
constexpr const char* st_applied = "APPLIED";
constexpr const char* st_rejected = "REJECTED";
constexpr const char* st_timeout = "TIMEOUT";

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
    uint64_t max_body_size = 64u << 20;
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
    time_t last_progress = 0;
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

  if (crypted)
    _server = http::server::load(
        _io_context, _logger, http_conf,
        [http_conf, io_ctx = _io_context, logger = _logger, weak]() {
          return std::make_shared<session<http::https_connection>>(
              io_ctx, logger, http_conf,
              http::https_connection::load_server_certificate, weak);
        });
  else
    _server = http::server::load(
        _io_context, _logger, http_conf,
        [http_conf, io_ctx = _io_context, logger = _logger, weak]() {
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

json api::_deployment_json(const deployment& d) const {
  return json{{"request_id", d.id},
              {"poller_id", std::to_string(d.poller_id)},
              {"status", d.status},
              {"message", d.message},
              {"config_version", d.config_version},
              {"plugins_missing", d.plugins_missing}};
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
                       {"connected_since", i.connected_since},
                       {"config_version",
                        v == _config_versions.end() ? "" : v->second}});
  }
  return json{{"pollers", pollers}};
}

/**
 * @brief A RemoteResult arrived from a poller (feeder thread of its stream).
 */
void api::_on_result(uint64_t poller_id, const RemoteResult& r) {
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
  _config_versions.erase(poller_id);
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
    if (absl::StartsWith(c.token, "encrypt::"))
      throw std::invalid_argument(
          "remote_api: encrypted token is not supported in the PoC");
    if (c.token.empty())
      throw std::invalid_argument("remote_api: a token is mandatory");
    c.public_cert = get("public_cert");
    c.private_key = get("private_key");
    if (!get("max_body_size").empty() &&
        !absl::SimpleAtoi(get("max_body_size"), &c.max_body_size))
      throw std::invalid_argument("remote_api: invalid max_body_size");

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
