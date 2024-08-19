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

#include "com/centreon/engine/configuration/extended_conf.hh"
#include "com/centreon/exceptions/msg_fmt.hh"
#include "common/engine_legacy_conf/state.hh"
#include "common/log_v2/log_v2.hh"

using namespace com::centreon::engine::configuration;
using com::centreon::common::log_v2::log_v2;

std::list<std::unique_ptr<extended_conf>> extended_conf::_confs;

/**
 * @brief Construct a new extended state::extended state object
 *
 * @param path of the configuration file
 * @throw exception if json malformed
 */
extended_conf::extended_conf(const std::string& path)
    : _logger{log_v2::instance().get(log_v2::CONFIG)}, _path(path) {
  if (::stat(_path.c_str(), &_file_info)) {
    SPDLOG_LOGGER_ERROR(_logger, "can't access to {}", _path);
    throw exceptions::msg_fmt("can't access to {}", _path);
  }
  try {
    _content = common::rapidjson_helper::read_from_file(_path);
    SPDLOG_LOGGER_INFO(_logger, "extended conf file {} loaded", _path);
  } catch (const std::exception& e) {
    SPDLOG_LOGGER_ERROR(
        _logger,
        "extended_conf::extended_conf : fail to read json content from {}: {}",
        _path, e.what());
    throw;
  }
}

/**
 * @brief checks if the file has been updated.
 * In that case, file is parsed. In case of failure, we continue to use old
 * version
 *
 */
void extended_conf::reload() {
  struct stat file_info;
  if (::stat(_path.c_str(), &file_info)) {
    SPDLOG_LOGGER_ERROR(
        _logger, "can't access to {} anymore => we keep old content", _path);
    return;
  }
  if (!memcmp(&file_info, &_file_info, sizeof(struct stat))) {
    return;
  }
  try {
    _content = common::rapidjson_helper::read_from_file(_path);
    _file_info = file_info;
  } catch (const std::exception& e) {
    SPDLOG_LOGGER_ERROR(_logger,
                        "extended_conf::extended_conf : fail to read json "
                        "content from {} => we keep old content, cause: {}",
                        _path, e.what());
  }
}

/**
 * @brief reload all optional configuration files if needed
 * Then these configuration content are applied to dest
 *
 * @param dest
 */
void extended_conf::update_state(state& dest) {
  for (auto& conf_file : _confs) {
    conf_file->reload();
    dest.apply_extended_conf(conf_file->_path, conf_file->_content);
  }
}
