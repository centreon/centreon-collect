/**
 * Copyright 2023-2024 Centreon (https://www.centreon.com/)
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * For more information : contact@centreon.com
 *
 */
#ifndef CCE_CONFIGURATION_WHITELIST_HH
#define CCE_CONFIGURATION_WHITELIST_HH

#include "common/log_v2/log_v2.hh"

namespace com::centreon::engine::configuration {
using com::centreon::common::log_v2::log_v2;

extern const std::string command_blacklist_output;

/**
 * @brief this struct is used to store the regex and wildcards.
 */
struct rule_set {
  std::vector<std::unique_ptr<re2::RE2>> regex;
  std::vector<std::string> wildcard;
};

/**
 * @brief this struct is used to store the cma-whitelist rules.
 * cma-whitelist is a special whitelist that is used by the CMA to restrict
 * commands that can be executed.
 */
struct cma_whitlist {
  rule_set defaults;
  std::unordered_map<std::string, rule_set> hosts;
};

/**
 * @brief this struct is used to store the top-level whitelist configuration.
 * It contains the main whitelist rules and an optional cma-whitelist.
 */
struct whitelist_config {
  rule_set engine;
  cma_whitlist cma;
};

/**
 * @brief the goal of this class is to parse yaml or json file with the
structure whitelist: wildcard:
    - /usr/lib/centreon/plugins/centreon_toto*titi
    - /usr/lib/centreon/plugins/centreon_toto*tata*
  regex:
    - /usr/lib/centreon/plugins/centreon_\d{5}.*
    -  /usr/lib/centreon/plugins/check_centreon_bam

When this kind of file is present in /etc/centreon-engine-whitelist directory
commands are executed only if they match to at least one wildcard or regex
string
 *
 */
class whitelist {
  static std::atomic_uint _instance_gen;
  std::shared_ptr<spdlog::logger> _logger;

  // don't reorder values
  enum e_refresh_result { no_directory, empty_directory, no_rule, rules };

  /**
   * @brief this id is used by checkable in oder to know is whitelist has been
   * reloaded
   *
   */
  uint _instance_id;
  whitelist_config _config;
  static std::unique_ptr<whitelist> _instance;

  template <class ryml_tree>
  bool _read_file_content(const ryml_tree& file_content);

  bool _parse_file(const std::string_view& file_path);

  static void init_ryml_error_handler();

  e_refresh_result parse_dir(const std::string_view directory);

  bool _empty(const rule_set& rules) const {
    return rules.wildcard.empty() && rules.regex.empty();
  }
  bool _is_allowed(const std::string& cmdline, const rule_set& rules);

 public:
  template <typename string_iter>
  whitelist(string_iter dir_path_begin, string_iter dir_path_end);

  whitelist(const std::string_view& file_path);

  static whitelist& instance();
  static void reload();

  bool empty_engine() const { return _empty(_config.engine); }

  bool empty_cma() const {
    return (_empty(_config.cma.defaults) && _config.cma.hosts.empty());
  }

  bool is_allowed_engine(const std::string& cmdline) {
    return _is_allowed(cmdline, _config.engine);
  }

  bool is_allowed_cma(const std::string& cmdline, const std::string& hostname);

  uint instance_id() const { return _instance_id; }

  const std::vector<std::string> get_wildcards() const {
    return _config.engine.wildcard;
  }
};

template <typename string_iter>
whitelist::whitelist(string_iter dir_path_begin, string_iter dir_path_end)
    : _logger{log_v2::instance().get(log_v2::CONFIG)},
      _instance_id{_instance_gen.fetch_add(1)} {
  init_ryml_error_handler();
  e_refresh_result res = e_refresh_result::no_directory;
  for (; dir_path_begin != dir_path_end; ++dir_path_begin) {
    e_refresh_result new_res = parse_dir(*dir_path_begin);
    if (new_res > res)
      res = new_res;
  }
  switch (res) {
    case e_refresh_result::no_directory:
      SPDLOG_LOGGER_INFO(
          _logger, "no whitelist directory found, all commands are accepted");
      break;
    case e_refresh_result::empty_directory:
    case e_refresh_result::no_rule:
      SPDLOG_LOGGER_INFO(_logger,
                         "whitelist directory found, but no restrictions, "
                         "all commands are accepted");
      break;
    default:
      break;
  }
}

}  // namespace com::centreon::engine::configuration

#endif
