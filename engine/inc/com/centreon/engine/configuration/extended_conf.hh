/**
 * Copyright 2022-2024 Centreon
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

#ifndef CCE_CONFIGURATION_EXTENDED_STATE_HH
#define CCE_CONFIGURATION_EXTENDED_STATE_HH

#include "com/centreon/common/rapidjson_helper.hh"
#include "common/engine_conf/state_helper.hh"

namespace com::centreon::engine::configuration {

class state;

/**
 * @brief contain json data of a config file passed in param to centengine
 * command line
 *
 */
class extended_conf {
  std::shared_ptr<spdlog::logger> _logger;
  std::string _path;
  struct stat _file_info;
  rapidjson::Document _content;

  static std::list<std::unique_ptr<extended_conf>> _confs;

 public:
  extended_conf(const std::string& path);
  ~extended_conf() = default;
  extended_conf(const extended_conf&) = delete;
  extended_conf& operator=(const extended_conf&) = delete;
  void reload();

  static void update_state(State* pb_config);

  template <class file_path_iterator>
  static void load_all(file_path_iterator begin, file_path_iterator);
};

/**
 * @brief try to load all extra configuration files
 * if one or more fail, we continue
 *
 * @tparam file_path_iterator
 * @param begin
 * @param end
 */
template <class file_path_iterator>
void extended_conf::load_all(file_path_iterator begin, file_path_iterator end) {
  _confs.clear();
  for (; begin != end; ++begin) {
    try {
      _confs.emplace_back(std::make_unique<extended_conf>(*begin));
    } catch (const std::exception&) {
    }
  }
}

}  // namespace com::centreon::engine::configuration

#endif
