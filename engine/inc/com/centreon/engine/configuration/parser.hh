/*
** Copyright 2011-2013,2017 Centreon
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

#ifndef CCE_CONFIGURATION_PARSER_HH
#define CCE_CONFIGURATION_PARSER_HH

#include <common/configuration/state.pb.h>
#include <fstream>
#include "com/centreon/engine/configuration/command.hh"
#include "com/centreon/engine/configuration/connector.hh"
#include "com/centreon/engine/configuration/contact.hh"
#include "com/centreon/engine/configuration/file_info.hh"
#include "com/centreon/engine/configuration/host.hh"
#include "com/centreon/engine/configuration/hostdependency.hh"
#include "com/centreon/engine/configuration/hostescalation.hh"
#include "com/centreon/engine/configuration/object.hh"
#include "com/centreon/engine/configuration/service.hh"
#include "com/centreon/engine/configuration/servicedependency.hh"
#include "com/centreon/engine/configuration/serviceescalation.hh"
#include "com/centreon/engine/configuration/state.hh"
#include "com/centreon/engine/configuration/timeperiod.hh"
#include "common/configuration/message_helper.hh"

namespace com::centreon::engine {

namespace configuration {
using Message = ::google::protobuf::Message;
using pb_map_object =
    absl::flat_hash_map<std::string, std::unique_ptr<Message>>;
using pb_map_helper =
    absl::flat_hash_map<Message*, std::unique_ptr<message_helper>>;

class parser {
  void _parse_global_configuration(std::string const& path, State* pb_config);
  void _check_validity(const Message& msg, const char* const* mandatory) const;
  bool _is_registered(const Message& msg) const;
  void _cleanup(State* pb_config);

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
  void parse(const std::string& path, state& config);
  void parse(const std::string& path, State* pb_config);

 private:
  typedef void (parser::*store)(object_ptr obj);

  parser(parser const& right);
  parser& operator=(parser const& right);
  void _add_object(object_ptr obj);
  void _add_template(object_ptr obj);
  template <typename T>
  void _apply(const T& lst, void (parser::*pfunc)(const std::string&)) {
    for (auto& f : lst)
      (this->*pfunc)(f);
  }

  template <typename S, typename L>
  void _apply(const L& lst,
              S* state,
              void (parser::*pfunc)(const std::string&, S*)) {
    for (auto& f : lst)
      (this->*pfunc)(f, state);
  }

  file_info const& _get_file_info(object* obj) const;
  void _get_hosts_by_hostgroups(hostgroup const& hostgroups, list_host& hosts);
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
  void _parse_directory_configuration(const std::string& path,
                                      State* pb_config);
  void _parse_global_configuration(std::string const& path);
  void _parse_object_definitions(const std::string& path);
  void _parse_object_definitions(const std::string& path, State* pb_config);
  void _parse_resource_file(std::string const& path);
  void _parse_resource_file(const std::string& path, State* pb_config);
  void _resolve_template(State* pb_config);
  void _resolve_template(std::unique_ptr<message_helper>& msg_helper,
                         const pb_map_object& tmpls);
  void _resolve_template();
  void _merge(std::unique_ptr<message_helper>& msg_helper, Message* tmpl);
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

  std::array<pb_map_object, 19> _pb_templates;
  pb_map_helper _pb_helper;
};
}  // namespace configuration

}

#endif  // !CCE_CONFIGURATION_PARSER_HH
