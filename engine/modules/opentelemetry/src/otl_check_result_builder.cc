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

#include "com/centreon/engine/checks/checker.hh"
#include "com/centreon/engine/globals.hh"
#include "com/centreon/engine/host.hh"
#include "com/centreon/engine/notifier.hh"
#include "com/centreon/engine/service.hh"

#include "com/centreon/exceptions/msg_fmt.hh"

#include <spdlog/spdlog.h>
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
 * @param logger
 */
otl_check_result_builder::otl_check_result_builder(
    const std::string& cmd_line,
    const std::shared_ptr<spdlog::logger>& logger)
    : _cmd_line(cmd_line), _logger(logger) {}

/**
 * @brief create a otl_converter_config from a command line
 *
 * @param cmd_line
 * @return std::shared_ptr<otl_check_result_builder>
 */
std::shared_ptr<otl_check_result_builder> otl_check_result_builder::create(
    const std::string& cmd_line,
    const std::shared_ptr<spdlog::logger>& logger) {
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
      return std::make_shared<telegraf::nagios_check_result_builder>(cmd_line,
                                                                     logger);
    } else if (extractor_type == "centreon_agent") {
      return std::make_shared<centreon_agent::agent_check_result_builder>(
          cmd_line, logger);
    } else {
      throw exceptions::msg_fmt("unknown processor in {}", cmd_line);
    }
  } catch (const std::exception& e) {
    SPDLOG_LOGGER_ERROR(config_logger,
                        "fail to get opentelemetry check_result_builder "
                        "configuration from {}: {}",
                        cmd_line, e.what());
    throw;
  }
}

/**
 * @brief convert opentelemetry datas in check_result and post it to
 * checks::checker::instance() Caution, this function must be called from engine
 * main thread
 *
 * @param host
 * @param serv empty if result of host check
 * @param data_pts opentelemetry data points
 */
void otl_check_result_builder::process_data_pts(
    const std::string_view& hst,
    const std::string_view& serv,
    const metric_to_datapoints& data_pts) {
  check_source notifier_type = check_source::service_check;
  notifier* host_or_serv = nullptr;

  if (serv.empty()) {
    notifier_type = check_source::host_check;
    auto found = host::hosts.find(hst);
    if (found == host::hosts.end()) {
      SPDLOG_LOGGER_ERROR(_logger, "unknow host: {}", hst);
      return;
    }
    host_or_serv = found->second.get();
  } else {
    auto found = service::services.find(std::make_pair(hst, serv));
    if (found == service::services.end()) {
      SPDLOG_LOGGER_ERROR(_logger, "unknow service {} for host", serv, hst);
      return;
    }
    host_or_serv = found->second.get();
  }
  timeval zero = {0, 0};
  std::shared_ptr<check_result> res = std::make_shared<check_result>(
      notifier_type, host_or_serv, checkable::check_type::check_passive,
      CHECK_OPTION_NONE, false, 0, zero, zero, false, true, 0, "");
  if (build_result_from_metrics(data_pts, *res)) {
    checks::checker::instance().add_check_result_to_reap(res);
  } else {
    SPDLOG_LOGGER_ERROR(
        _logger,
        "fail to convert opentelemetry datas in centreon check_result for host "
        "{}, serv {}",
        hst, serv);
  }
}

/**
 * @brief debug infos
 *
 * @param output string to log
 */
void otl_check_result_builder::dump(std::string& output) const {
  output = _cmd_line;
}
