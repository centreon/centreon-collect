/*
** Copyright 2024 Centreon
**
** This file is part of Centreon Engine.
**
** Centreon Engine is free software: you can redistribute it and/or
** modify it under the terms of the GNU General Public License version 2
** as published by the Free Software Foundation.
**
** Centreon Engine is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
** General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with Centreon Engine. If not, see
** <http://www.gnu.org/licenses/>.
*/

#ifndef CCE_CONFIGURATION_EXTENDED_STATE_HH
#define CCE_CONFIGURATION_EXTENDED_STATE_HH

#include "com/centreon/common/rapidjson_helper.hh"

namespace com::centreon::engine::configuration {

class state;

/**
 * @brief contain json data of a config file passed in param to centengine
 * command line
 *
 */
class extended_conf {
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

  static void apply_all_to_state(state& dest);

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
