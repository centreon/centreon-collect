/**
 * Copyright 2011-2013,2017-2024 Centreon
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
#ifndef CCE_CONFIGURATION_PARSER_HH
#define CCE_CONFIGURATION_PARSER_HH

#include <fstream>
#include "file_info.hh"
#include "state.hh"
#include "host.hh"

namespace com::centreon::engine::configuration {

class parser {
  std::shared_ptr<spdlog::logger> _logger;

 public:
  enum read_options {
    read_commands = (1 << 0),
    read_connector = (1 << 1),
    read_contact = (1 << 2),
    read_contactgroup = (1 << 3),
    read_host = (1 << 4),
    read_hostdependency = (1 << 5),
    read_hostescalation = (1 << 6),
    read_hostgroup = (1 << 8),
    read_hostgroupescalation = (1 << 9),
    read_service = (1 << 10),
    read_servicedependency = (1 << 11),
    read_serviceescalation = (1 << 12),
    read_servicegroup = (1 << 14),
    read_timeperiod = (1 << 15),
    read_all = (~0)
  };

  parser(unsigned int read_options = read_all);
  ~parser() noexcept = default;
  void parse(const std::string& path, state& config, error_cnt& err);

 private:
  typedef void (parser::*store)(object_ptr obj);

  parser(parser const& right);
  parser& operator=(parser const& right);
  void _add_object(object_ptr obj);
  void _add_template(object_ptr obj);
  void _apply(std::list<std::string> const& lst,
              void (parser::*pfunc)(std::string const&));
  file_info const& _get_file_info(object* obj) const;
  void _get_hosts_by_hostgroups(const hostgroup& hostgroups,
                                list_host& hosts);
  void _get_hosts_by_hostgroups_name(set_string const& lst_group,
                                     list_host& hosts);
  template <typename T>
  void _get_objects_by_list_name(set_string const& lst,
                                 map_object& objects,
                                 std::list<T>& out);

  template <typename T>
  static void _insert(list_object const& from, std::set<T>& to);
  template <typename T>
  static void _insert(map_object const& from, std::set<T>& to);
  std::string const& _map_object_type(map_object const& objects) const throw();
  void _parse_directory_configuration(std::string const& path);
  void _parse_global_configuration(const std::string& path);
  void _parse_object_definitions(const std::string& path);
  void _parse_resource_file(std::string const& path);
  void _resolve_template(error_cnt& err);
  void _store_into_list(object_ptr obj);
  template <typename T, std::string const& (T::*ptr)() const throw()>
  void _store_into_map(object_ptr obj);

  state* _config;
  unsigned int _current_line;
  std::string _current_path;
  std::array<list_object, 19> _lst_objects;
  std::array<map_object, 19> _map_objects;
  std::unordered_map<object*, file_info> _objects_info;
  unsigned int _read_options;
  static store _store[];
  std::array<map_object, 19> _templates;
};
}  // namespace com::centreon::engine::configuration

#endif  // !CCE_CONFIGURATION_PARSER_HH
