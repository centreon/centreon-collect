/**
 * Copyright 2024 Centreon
 *
 * This file is part of Centreon Engine.
 *
 * Centreon Engine is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation.
 *
 * Centreon Engine is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Centreon Engine. If not, see
 * <http://www.gnu.org/licenses/>.
 */

#include "com/centreon/engine/configuration/extended_conf.hh"
#include "com/centreon/engine/configuration/state.hh"
#include "com/centreon/exceptions/msg_fmt.hh"

using namespace com::centreon::engine::configuration;

std::list<std::unique_ptr<extended_conf>> extended_conf::_confs;

/**
 * @brief Construct a new extended state::extended state object
 *
 * @param path
 * @throw exception if json mal formed
 */
extended_conf::extended_conf(const std::string& path) : _path(path) {
  if (::stat(_path.c_str(), &_file_info)) {
    SPDLOG_LOGGER_ERROR(log_v2::config(), "can't access to {}", _path);
    throw exceptions::msg_fmt("can't access to {}", _path);
  }
  try {
    _content = common::rapidjson_helper::read_from_file(_path);
  } catch (const std::exception& e) {
    SPDLOG_LOGGER_ERROR(
        log_v2::config(),
        "extended_conf::extended_conf : fail to read json content from {}",
        _path);
    throw;
  }
}

void extended_conf::reload() {
  struct stat file_info;
  if (::stat(_path.c_str(), &file_info)) {
    SPDLOG_LOGGER_ERROR(log_v2::config(),
                        "can't access to {} anymore => we keep old content",
                        _path);
    return;
  }
  if (::stat(_path.c_str(), &_file_info)) {
    SPDLOG_LOGGER_ERROR(log_v2::config(), "can't access to {}", _path);
    return;
  }
  if (!memcmp(&file_info, &_file_info, sizeof(struct stat))) {
    return;
  }
  try {
    _content = common::rapidjson_helper::read_from_file(_path);
    _file_info = file_info;
  } catch (const std::exception& e) {
    SPDLOG_LOGGER_ERROR(log_v2::config(),
                        "extended_conf::extended_conf : fail to read json "
                        "content from {} => we keep old content",
                        _path);
  }
}

void extended_conf::apply_all_to_state(state& dest) {
  for (auto& conf_file : _confs) {
    conf_file->reload();
    dest.apply_extended_conf(conf_file->_path, conf_file->_content);
  }
}
