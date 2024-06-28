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
#include "otl_check_result_builder.hh"

#include "centreon_agent/agent_check_result_builder.hh"
#include "telegraf/nagios_check_result_builder.hh"

#include "absl/flags/commandlineflag.h"
#include "absl/strings/str_split.h"

using namespace com::centreon::engine::modules::opentelemetry;

/**
 * @brief Construct a new otl check result builder::otl check result builder
 * object
 *
 * @param cmd_line
 * @param command_id
 * @param host
 * @param service
 * @param timeout
 * @param handler called when mandatory metrics will be available
 * @param logger
 */
otl_check_result_builder::otl_check_result_builder(
    const std::string& cmd_line,
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

/**
 * @brief try to build a check result
 *
 * @param data_pts
 * @param res
 * @return true all mandatory metrics are available and a check_result is built
 * @return false
 */
bool otl_check_result_builder::sync_build_result_from_metrics(
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
 * @return true otl_check_result_builder has managed to create check result
 * @return false
 */
bool otl_check_result_builder::async_build_result_from_metrics(
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
void otl_check_result_builder::async_time_out() {
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
 * std::shared_ptr<otl_check_result_builder> converter =
 * otl_check_result_builder::create("--processor=nagios_telegraf
 * --fifo_depth=5", conf, 5, *host, serv, timeout_point, [](const
 * commads::result &res){}, _logger);
 * @endcode
 *
 * @param cmd_line
 * @param conf  bean configuration object created by
 * create_check_result_builder_config
 * @param command_id
 * @param host
 * @param service
 * @param timeout
 * @param handler handler that will be called once we have all metrics mandatory
 * to create a check_result
 * @return std::shared_ptr<otl_check_result_builder>
 */
std::shared_ptr<otl_check_result_builder> otl_check_result_builder::create(
    const std::string& cmd_line,
    const std::shared_ptr<check_result_builder_config>& conf,
    uint64_t command_id,
    const host& host,
    const service* service,
    std::chrono::system_clock::time_point timeout,
    commands::otel::result_callback&& handler,
    const std::shared_ptr<spdlog::logger>& logger) {
  switch (conf->get_type()) {
    case check_result_builder_config::converter_type::
        nagios_check_result_builder:
      return std::make_shared<telegraf::nagios_check_result_builder>(
          cmd_line, command_id, host, service, timeout, std::move(handler),
          logger);
    case check_result_builder_config::converter_type::
        centreon_agent_check_result_builder:
      return std::make_shared<centreon_agent::agent_check_result_builder>(
          cmd_line, command_id, host, service, timeout, std::move(handler),
          logger);
    default:
      SPDLOG_LOGGER_ERROR(logger, "unknown converter type:{}", cmd_line);
      throw exceptions::msg_fmt("unknown converter type:{}", cmd_line);
  }
}

/**
 * @brief debug infos
 *
 * @param output string to log
 */
void otl_check_result_builder::dump(std::string& output) const {
  output = fmt::format(
      "host:{}, service:{}, command_id={}, timeout:{} cmdline: \"{}\"",
      _host_serv.first, _host_serv.second, _command_id, _timeout, _cmd_line);
}

/**
 * @brief create a otl_converter_config from a command line
 * --processor flag identifies type of converter
 * Example:
 * @code {.c++}
 * std::shared_ptr<otl_converter> converter =
 * otl_converter::create_check_result_builder_config("--processor=nagios_telegraf
 * --fifo_depth=5");
 *
 * @param cmd_line
 * @return std::shared_ptr<check_result_builder_config>
 */
std::shared_ptr<check_result_builder_config>
otl_check_result_builder::create_check_result_builder_config(
    const std::string& cmd_line) {
  static initialized_data_class<po::options_description> desc(
      [](po::options_description& desc) {
        desc.add_options()("processor", po::value<std::string>(),
                           "processor type");
      });

  try {
    po::variables_map vm;
    po::store(po::command_line_parser(po::split_unix(cmd_line))
                  .options(desc)
                  .allow_unregistered()
                  .run(),
              vm);
    if (!vm.count("processor")) {
      throw exceptions::msg_fmt("processor flag not found in {}", cmd_line);
    }
    std::string extractor_type = vm["processor"].as<std::string>();
    if (extractor_type == "nagios_telegraf") {
      return std::make_shared<check_result_builder_config>(
          check_result_builder_config::converter_type::
              nagios_check_result_builder);
    } else if (extractor_type == "centreon_agent") {
      return std::make_shared<check_result_builder_config>(
          check_result_builder_config::converter_type::
              centreon_agent_check_result_builder);
    } else {
      throw exceptions::msg_fmt("unknown processor in {}", cmd_line);
    }
  } catch (const std::exception& e) {
    SPDLOG_LOGGER_ERROR(
        config_logger,
        "fail to get opentelemetry converter configuration from {}: {}",
        cmd_line, e.what());
    throw;
  }
}
