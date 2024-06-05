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

#include "com/centreon/engine/globals.hh"
#include "com/centreon/exceptions/msg_fmt.hh"

#include "data_point_fifo_container.hh"
#include "otl_converter.hh"

#include "absl/flags/commandlineflag.h"
#include "absl/strings/str_split.h"

using namespace com::centreon::engine::modules::opentelemetry;

/**
 * @brief create a otl_converter_config from a command line
 * first field identify type of config
 * Example:
 * @code {.c++}
 * std::shared_ptr<otl_converter_config> conf =
 * otl_converter_config::load("nagios_telegraf");
 * @endcode
 *
 * @param cmd_line
 * @return std::shared_ptr<otl_converter_config>
 * @throw if cmd_line can't be parsed
 */
/******************************************************************
 * otl_converter base
 ******************************************************************/
otl_converter::otl_converter(const std::string& cmd_line,
                             uint64_t command_id,
                             const host& host,
                             const service* service,
                             std::chrono::system_clock::time_point timeout,
                             commands::otel::result_callback&& handler,
                             const std::shared_ptr<spdlog::logger>& logger)
    : _cmd_line(cmd_line),
      _command_id(command_id),
      _host_serv{host.name(), service ? service->description() : ""},
      _timeout(timeout),
      _callback(handler),
      _logger(logger) {}

bool otl_converter::sync_build_result_from_metrics(
    data_point_fifo_container& data_pts,
    commands::result& res) {
  std::lock_guard l(data_pts);
  auto& fifos = data_pts.get_fifos(_host_serv.first, _host_serv.second);
  if (!fifos.empty() && _build_result_from_metrics(fifos, res)) {
    return true;
  }
  // no data available
  return false;
}

/**
 * @brief called  when data is received from otel
 * clients
 *
 * @param data_pts
 * @return true otl_converter has managed to create check result
 * @return false
 */
bool otl_converter::async_build_result_from_metrics(
    data_point_fifo_container& data_pts) {
  commands::result res;
  bool success = false;
  {
    std::lock_guard l(data_pts);
    auto& fifos = data_pts.get_fifos(_host_serv.first, _host_serv.second);
    success = !fifos.empty() && _build_result_from_metrics(fifos, res);
  }
  if (success) {
    _callback(res);
  }
  return success;
}

/**
 * @brief called  when no data is received before
 * _timeout
 *
 */
void otl_converter::async_time_out() {
  commands::result res;
  res.exit_status = process::timeout;
  res.command_id = _command_id;
  _callback(res);
}

/**
 * @brief create a otl_converter_config from a command line
 * first field identify type of config
 * Example:
 * @code {.c++}
 * std::shared_ptr<otl_converter> converter =
 * otl_converter::create("nagios_telegraf --fifo_depth=5", 5, *host, serv,
 * timeout_point, [](const commads::result &res){}, _logger);
 * @endcode
 *
 * @param cmd_line
 * @param command_id
 * @param host
 * @param service
 * @param timeout
 * @param handler
 * @return std::shared_ptr<otl_converter>
 */
std::shared_ptr<otl_converter> otl_converter::create(
    const std::string& cmd_line,
    uint64_t command_id,
    const host& host,
    const service* service,
    std::chrono::system_clock::time_point timeout,
    commands::otel::result_callback&& handler,
    const std::shared_ptr<spdlog::logger>& logger) {
  // type of the converter is the first field
  size_t sep_pos = cmd_line.find(' ');
  std::string conf_type =
      sep_pos == std::string::npos ? cmd_line : cmd_line.substr(0, sep_pos);
  boost::trim(conf_type);
  // NEXT PR
  // if (conf_type == "nagios_telegraf") {
  //   return std::make_shared<telegraf::otl_nagios_converter>(
  //       cmd_line, command_id, host, service, timeout, std::move(handler),
  //       logger);
  // } else {
  SPDLOG_LOGGER_ERROR(config_logger, "unknown converter type:{}", conf_type);
  throw exceptions::msg_fmt("unknown converter type:{}", conf_type);
  // }
}

/**
 * @brief debug infos
 *
 * @param output string to log
 */
void otl_converter::dump(std::string& output) const {
  output = fmt::format(
      "host:{}, service:{}, command_id={}, timeout:{} cmdline: \"{}\"",
      _host_serv.first, _host_serv.second, _command_id, _timeout, _cmd_line);
}

/**
 * @brief remove converter_type from command_line
 *
 * @param cmd_line exemple nagios_telegraf
 * @return std::string
 */
std::string otl_converter::remove_converter_type(const std::string& cmd_line) {
  size_t sep_pos = cmd_line.find(' ');
  std::string params =
      sep_pos == std::string::npos ? "" : cmd_line.substr(sep_pos + 1);

  boost::trim(params);
  return params;
}
