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
#include <google/protobuf/util/json_util.h>
#include "com/centreon/exceptions/msg_fmt.hh"
#ifdef LEGACY_CONF
#include "common/engine_legacy_conf/state.hh"
#else
#include "common/engine_conf/state_helper.hh"
#endif
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

#ifdef LEGACY_CONF
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
#else
/**
 * @brief reload all optional configuration files if needed
 * Then these configuration content are applied to dest
 *
 * @param dest
 */
void extended_conf::update_state(State* pb_config) {
  for (auto& conf_file : _confs) {
    conf_file->reload();
    std::ifstream f(conf_file->_path, std::ios::in);
    std::string content;
    if (f) {
      f.seekg(0, std::ios::end);
      content.resize(f.tellg());
      f.seekg(0, std::ios::beg);
      f.read(&content[0], content.size());
      f.close();
      State new_conf;
      google::protobuf::util::JsonParseOptions options;
      options.ignore_unknown_fields = false;
      options.case_insensitive_enum_parsing = true;
      auto status [[maybe_unused]] =
          google::protobuf::util::JsonStringToMessage(content, &new_conf);
      pb_config->MergeFrom(new_conf);
    } else {
      SPDLOG_LOGGER_ERROR(
          conf_file->_logger,
          "extended_conf::extended_conf : fail to read json content '{}': {}",
          conf_file->_path, strerror(errno));
    }
  }
}
#endif
