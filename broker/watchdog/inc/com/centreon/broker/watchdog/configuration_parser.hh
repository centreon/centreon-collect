/*
** Copyright 2015 Centreon
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
**
** For more information : contact@centreon.com
*/

#ifndef CCB_WATCHDOG_CONFIGURATION_PARSER_HH
#define CCB_WATCHDOG_CONFIGURATION_PARSER_HH

#include <nlohmann/json.hpp>

#include "com/centreon/broker/watchdog/configuration.hh"
#include "com/centreon/broker/watchdog/instance_configuration.hh"

namespace com::centreon::broker {

namespace watchdog {
/**
 *  @class configuration_parser configuration_parser.hh
 * "com/centreon/broker/watchdog/configuration_parser.hh"
 *  @brief Watchdog configuration parser.
 *
 *  Parse watchdog configuration.
 */
class configuration_parser {
  nlohmann::json _json_document;
  std::string _log_path;
  configuration::instance_map _instances_configuration;

  void _parse_file(std::string const& config_filename);
  void _check_json_document();
  void _parse_centreon_broker_element(const nlohmann::json& element);

 public:
  configuration_parser();
  ~configuration_parser();
  configuration_parser& operator=(const configuration_parser&) = delete;
  configuration_parser(const configuration_parser&) = delete;

  configuration parse(std::string const& config_filename);
};
}  // namespace watchdog

}

#endif  // !CCB_WATCHDOG_CONFIGURATION_PARSER_HH
