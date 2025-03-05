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

#include <boost/url.hpp>

#include "conf_helper.hh"
#include "telegraf/conf_server.hh"

#include "com/centreon/engine/globals.hh"
#include "com/centreon/engine/host.hh"
#include "com/centreon/engine/macros.hh"
#include "com/centreon/engine/service.hh"

#include "com/centreon/common/http/https_connection.hh"
#include "com/centreon/engine/command_manager.hh"
#include "com/centreon/engine/commands/forward.hh"

#include "com/centreon/exceptions/msg_fmt.hh"

using namespace com::centreon::engine::modules::opentelemetry::telegraf;
using namespace com::centreon::engine;

static constexpr std::string_view _config_schema(R"(
{
    "$schema": "http://json-schema.org/draft-04/schema#",
    "title": "telegraf config",
    "properties": {
        "http_server" : {
          "listen_address": {
              "description": "IP",
              "type": "string",
              "minLength": 5
          },
          "port": {
              "description": "port to listen",
              "type": "integer",
              "minimum": 80,
              "maximum": 65535
          },
          "encryption": {
              "description": "true if https",
              "type": "boolean"
          },
          "keepalive_interval": {
              "description": "delay between 2 keepalive tcp packet, 0 no keepalive packets",
              "type": "integer",
              "minimum": 0,
              "maximum": 3600
          },
          "public_cert": {
              "description": "path of the certificate file of the server",
              "type": "string",
              "minLength": 5
          },
          "private_key": {
              "description": "path of the key file",
              "type": "string",
              "minLength": 5
          }
        },
        "check_interval": {
            "description": "interval in seconds between two checks (param [agent] interval) ",
            "type": "integer",
            "minimum": 10
        }
    },
    "type": "object"
}
)");

conf_server_config::conf_server_config(const rapidjson::Value& json_config_v,
                                       asio::io_context& io_context) {
  common::rapidjson_helper json_config(json_config_v);

  static common::json_validator validator(_config_schema);
  try {
    json_config.validate(validator);
  } catch (const std::exception& e) {
    SPDLOG_LOGGER_ERROR(config_logger,
                        "forbidden values in telegraf_conf_server config: {}",
                        e.what());
    throw;
  }

  _check_interval = json_config.get_unsigned("check_interval", 60);

  if (json_config_v.HasMember("http_server")) {
    common::rapidjson_helper http_json_config(
        json_config.get_member("http_server"));
    std::string listen_address =
        http_json_config.get_string("listen_address", "0.0.0.0");
    _crypted = http_json_config.get_bool("encryption", false);
    unsigned port = http_json_config.get_unsigned("port", _crypted ? 443 : 80);
    asio::ip::tcp::resolver::query query(listen_address, std::to_string(port));
    asio::ip::tcp::resolver resolver(io_context);
    boost::system::error_code ec;
    asio::ip::tcp::resolver::iterator it = resolver.resolve(query, ec), end;
    if (ec) {
      throw exceptions::msg_fmt("unable to resolve {}:{}", listen_address,
                                port);
    }
    if (it == end) {
      throw exceptions::msg_fmt("no ip found for {}:{}", listen_address, port);
    }

    _listen_endpoint = it->endpoint();

    _second_keep_alive_interval =
        http_json_config.get_unsigned("keepalive_interval", 30);
    _public_cert = http_json_config.get_string("public_cert", "");
    _private_key = http_json_config.get_string("private_key", "");
    if (_crypted) {
      if (_public_cert.empty()) {
        SPDLOG_LOGGER_ERROR(config_logger,
                            "telegraf conf server  encryption activated and no "
                            "certificate path "
                            "provided");
        throw exceptions::msg_fmt(
            "telegraf conf server  encryption activated and no certificate "
            "path "
            "provided");
      }
      if (_private_key.empty()) {
        SPDLOG_LOGGER_ERROR(config_logger,
                            "telegraf conf server  encryption activated and no "
                            "certificate key path provided");

        throw exceptions::msg_fmt(
            "telegraf conf server  encryption activated and no certificate key "
            "path provided");
      }
      if (::access(_public_cert.c_str(), R_OK)) {
        SPDLOG_LOGGER_ERROR(
            config_logger,
            "telegraf conf server unable to read certificate file {}",
            _public_cert);
        throw exceptions::msg_fmt(
            "telegraf conf server unable to read certificate file {}",
            _public_cert);
      }
      if (::access(_private_key.c_str(), R_OK)) {
        SPDLOG_LOGGER_ERROR(
            config_logger,
            "telegraf conf server unable to read certificate key file {}",
            _private_key);
        throw exceptions::msg_fmt(
            "telegraf conf server unable to read certificate key file {}",
            _private_key);
      }
    }
  } else {
    _listen_endpoint = asio::ip::tcp::endpoint(asio::ip::address_v4::any(), 80);
    _second_keep_alive_interval = 30;
    _check_interval = 60;
    _crypted = false;
  }
}

bool conf_server_config::operator==(const conf_server_config& right) const {
  return _listen_endpoint == right._listen_endpoint &&
         _crypted == right._crypted &&
         _second_keep_alive_interval == right._second_keep_alive_interval &&
         _public_cert == right._public_cert &&
         _private_key == right._private_key &&
         _check_interval == right._check_interval;
}

/**********************************************************************
 *                       session http(s)
 **********************************************************************/

/**
 * @brief http/https session
 *
 * @tparam connection_class http_connection or https_connection
 */
template <class connection_class>
void conf_session<connection_class>::on_accept() {
  connection_class::_on_accept(
      [me = shared_from_this()](const boost::beast::error_code& err,
                                const std::string&) {
        if (!err)
          me->wait_for_request();
      });
}

/**
 * @brief after connection or have sent a response, it waits for a new incomming
 * request
 *
 * @tparam connection_class
 */
template <class connection_class>
void conf_session<connection_class>::wait_for_request() {
  connection_class::receive_request(
      [me = shared_from_this()](
          const boost::beast::error_code& err, const std::string&,
          const std::shared_ptr<http::request_type>& request) {
        if (err) {
          SPDLOG_LOGGER_DEBUG(me->_logger,
                              "fail to receive request from {}: {}", me->_peer,
                              err.what());
          return;
        }
        me->on_receive_request(request);
      });
}

/**
 * @brief incomming request handler
 * handler is passed to the main thread via command_manager::instance().enqueue
 *
 * @tparam connection_class
 * @param request
 */
template <class connection_class>
void conf_session<connection_class>::on_receive_request(
    const std::shared_ptr<http::request_type>& request) {
  boost::url_view parsed(request->target());
  std::string host;

  for (const auto& get_param : parsed.params()) {
    if (get_param.key == "host") {
      host = get_param.value;
    }
  }
  auto to_call = std::packaged_task<int(void)>(
      [me = shared_from_this(), request, host]() mutable -> int32_t {
        // then we are in the main thread
        // services, hosts and commands are stable
        me->answer_to_request(request, host);
        return 0;
      });
  command_manager::instance().enqueue(std::move(to_call));
}

/**
 * @brief add a nagios paragraph to telegraf configuration
 *
 * @param cmd_name name of the command
 * @param cmd_line command line configured and filled by macr contents
 * @param host host name
 * @param service  service name
 * @param to_append response body
 * @return true a not empty command line is present after --cmd_line flag
 * @return false
 */
template <class connection_class>
bool conf_session<connection_class>::_otel_connector_to_stream(
    const std::string& cmd_name,
    const std::string& cmd_line,
    const std::string& host,
    const std::string& service,
    std::string& to_append) {
  std::string plugins_cmdline = boost::trim_copy(cmd_line);

  if (plugins_cmdline.empty()) {
    SPDLOG_LOGGER_ERROR(this->_logger,
                        "host: {}, serv: {}, no plugins cmd_line found in {}",
                        host, service, cmd_line);
    return false;
  }

  SPDLOG_LOGGER_DEBUG(this->_logger,
                      "host: {}, serv: {}, cmd {} plugins cmd_line {}", host,
                      service, cmd_name, cmd_line);

  fmt::format_to(std::back_inserter(to_append), R"(
[[inputs.exec]]
  name_override = "{}"
  commands = ["{}"]
  data_format = "nagios"
  [inputs.exec.tags]
    host = "{}"
    service = "{}"

)",
                 cmd_name, plugins_cmdline, host, service);
  return true;
}

/**
 * @brief construct and send conf to telegraf
 * As it uses host, services and command list, it must be called in the main
 * thread
 * host::hosts and service::services must be stable during this call
 *
 * @tparam connection_class
 * @param request incoming request
 * @param host_list hosts extracted from get parameters
 */
template <class connection_class>
void conf_session<connection_class>::answer_to_request(
    const std::shared_ptr<http::request_type>& request,
    const std::string& host) {
  http::response_ptr resp(std::make_shared<http::response_type>());
  resp->version(request->version());

  resp->body() = fmt::format(R"(# Centreon telegraf configuration
# This telegraf configuration is generated by centreon centengine
[agent]
  ## Default data collection interval for all inputs
  interval = "{}s"

)",
                             _telegraf_conf->get_check_interval());

  whitelist_cache wcache;

  bool at_least_one_found = get_otel_commands(
      host,
      [this, &resp, &host](
          const std::string& cmd_name, const std::string& cmd_line,
          const std::string& service,
          [[maybe_unused]] const std::shared_ptr<spdlog::logger>& logger) {
        return _otel_connector_to_stream(cmd_name, cmd_line, host, service,
                                         resp->body());
      },
      wcache, this->_logger);

  if (at_least_one_found) {
    resp->result(boost::beast::http::status::ok);
    resp->insert(boost::beast::http::field::content_type, "text/plain");
  } else {
    resp->result(boost::beast::http::status::not_found);
    resp->body() =
        "<html><body>No host service found from get parameters</body></html>";
  }
  resp->content_length(resp->body().length());

  connection_class::answer(
      resp, [me = shared_from_this()](const boost::beast::error_code& err,
                                      const std::string& detail) {
        if (err) {
          SPDLOG_LOGGER_ERROR(me->_logger, "fail to answer to telegraf {} {}",
                              err.message(), detail);
          return;
        }
        me->wait_for_request();
      });
}

namespace com::centreon::engine::modules::opentelemetry::telegraf {
template class conf_session<http::http_connection>;
template class conf_session<http::https_connection>;
};  // namespace com::centreon::engine::modules::opentelemetry::telegraf
