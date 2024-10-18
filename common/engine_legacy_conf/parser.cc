/**
 * Copyright 2011-2014,2017-2024 Centreon
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
#include "parser.hh"
#include <filesystem>
#include "common/log_v2/log_v2.hh"

using namespace com::centreon;
using namespace com::centreon::engine::configuration;
using com::centreon::common::log_v2::log_v2;
using com::centreon::exceptions::msg_fmt;

parser::store parser::_store[] = {
    &parser::_store_into_map<command, &command::command_name>,
    &parser::_store_into_map<connector, &connector::connector_name>,
    &parser::_store_into_map<contact, &contact::contact_name>,
    &parser::_store_into_map<contactgroup, &contactgroup::contactgroup_name>,
    &parser::_store_into_map<host, &host::host_name>,
    &parser::_store_into_list,
    &parser::_store_into_list,
    &parser::_store_into_list,
    &parser::_store_into_map<hostgroup, &hostgroup::hostgroup_name>,
    &parser::_store_into_list,
    &parser::_store_into_list,
    &parser::_store_into_list,
    &parser::_store_into_list,
    &parser::_store_into_map<servicegroup, &servicegroup::servicegroup_name>,
    &parser::_store_into_map<timeperiod, &timeperiod::timeperiod_name>,
    &parser::_store_into_list,
    &parser::_store_into_list,
    &parser::_store_into_list};

/**
 *  Get the next valid line.
 *
 *  @param[in, out] stream The current stream to read new line.
 *  @param[out]     line   The line to fill.
 *  @param[in, out] pos    The current position.
 *
 *  @return True if data is available, false if no data.
 */
static bool get_next_line(std::ifstream& stream,
                          std::string& line,
                          uint32_t& pos) {
  while (std::getline(stream, line, '\n')) {
    ++pos;
    line = absl::StripAsciiWhitespace(line);
    if (!line.empty()) {
      char c = line[0];
      if (c != '#' && c != ';' && c != '\x0')
        return true;
    }
  }
  return false;
}

/**
 *  Default constructor.
 *
 *  @param[in] read_options Configuration file reading options
 *             (use to skip some object type).
 */
parser::parser(unsigned int read_options)
    : _logger{log_v2::instance().get(log_v2::CONFIG)},
      _config(nullptr),
      _read_options(read_options) {}

/**
 *  Parse configuration file.
 *
 *  @param[in] path   The configuration file path.
 *  @param[in] config The state configuration to fill.
 */
void parser::parse(std::string const& path, state& config, error_cnt& err) {
  _config = &config;

  // parse the global configuration file.
  _parse_global_configuration(path);

  // parse configuration files.
  _apply(config.cfg_file(), &parser::_parse_object_definitions);
  // parse resource files.
  _apply(config.resource_file(), &parser::_parse_resource_file);
  // parse configuration directories.
  _apply(config.cfg_dir(), &parser::_parse_directory_configuration);

  // Apply template.
  _resolve_template(err);

  // Fill state.
  _insert(_map_objects[object::command], config.commands());
  _insert(_map_objects[object::connector], config.connectors());
  _insert(_map_objects[object::contact], config.contacts());
  _insert(_map_objects[object::contactgroup], config.contactgroups());
  _insert(_lst_objects[object::hostdependency], config.hostdependencies());
  _insert(_lst_objects[object::hostescalation], config.hostescalations());
  _insert(_map_objects[object::hostgroup], config.hostgroups());
  _insert(_map_objects[object::host], config.hosts());
  _insert(_lst_objects[object::servicedependency],
          config.servicedependencies());
  _insert(_lst_objects[object::serviceescalation], config.serviceescalations());
  _insert(_map_objects[object::servicegroup], config.servicegroups());
  _insert(_lst_objects[object::service], config.mut_services());
  _insert(_lst_objects[object::anomalydetection], config.anomalydetections());
  _insert(_map_objects[object::timeperiod], config.timeperiods());
  _insert(_lst_objects[object::severity], config.mut_severities());
  _insert(_lst_objects[object::tag], config.mut_tags());

  // cleanup.
  _objects_info.clear();
  for (unsigned int i(0); i < _lst_objects.size(); ++i) {
    _lst_objects[i].clear();
    _map_objects[i].clear();
    _templates[i].clear();
  }
}

/**
 *  Add object into the list.
 *
 *  @param[in] obj The object to add into the list.
 */
void parser::_add_object(object_ptr obj) {
  if (obj->should_register())
    (this->*_store[obj->type()])(obj);
}

/**
 *  Add template into the list.
 *
 *  @param[in] obj The tempalte to add into the list.
 */
void parser::_add_template(object_ptr obj) {
  std::string const& name(obj->name());
  if (name.empty())
    throw msg_fmt("Parsing of {} failed {}: Property 'name' is missing",
                  obj->type_name(), _get_file_info(obj.get()));
  map_object& tmpl(_templates[obj->type()]);
  if (tmpl.find(name) != tmpl.end())
    throw msg_fmt("Parsing of {} failed {}: {} already exists",
                  obj->type_name(), _get_file_info(obj.get()), name);
  tmpl[name] = obj;
}

/**
 *  Apply parse method into list.
 *
 *  @param[in] lst   The list to apply action.
 *  @param[in] pfunc The method to apply.
 */
void parser::_apply(std::list<std::string> const& lst,
                    void (parser::*pfunc)(std::string const&)) {
  for (std::list<std::string>::const_iterator it(lst.begin()), end(lst.end());
       it != end; ++it)
    (this->*pfunc)(*it);
}

/**
 *  Get the file information.
 *
 *  @param[in] obj The object to get file informations.
 *
 *  @return The file informations object.
 */
file_info const& parser::_get_file_info(object* obj) const {
  if (obj) {
    std::unordered_map<object*, file_info>::const_iterator it(
        _objects_info.find(obj));
    if (it != _objects_info.end())
      return it->second;
  }
  throw msg_fmt(
      "Parsing failed: Object not found into the file information cache");
}

/**
 *  Build the hosts list with hostgroups.
 *
 *  @param[in]     hostgroups The hostgroups.
 *  @param[in,out] hosts      The host list to fill.
 */
void parser::_get_hosts_by_hostgroups(hostgroup const& hostgroups,
                                      list_host& hosts) {
  _get_objects_by_list_name(hostgroups.members(), _map_objects[object::host],
                            hosts);
}

/**
 *  Build the hosts list with list of hostgroups.
 *
 *  @param[in]     hostgroups The hostgroups list.
 *  @param[in,out] hosts      The host list to fill.
 */
void parser::_get_hosts_by_hostgroups_name(set_string const& lst_group,
                                           list_host& hosts) {
  map_object& gl_hostgroups(_map_objects[object::hostgroup]);
  for (set_string::const_iterator it(lst_group.begin()), end(lst_group.end());
       it != end; ++it) {
    map_object::iterator it_hostgroups(gl_hostgroups.find(*it));
    if (it_hostgroups != gl_hostgroups.end())
      _get_hosts_by_hostgroups(
          *static_cast<configuration::hostgroup*>(it_hostgroups->second.get()),
          hosts);
  }
}

/**
 *  Build the object list with list of object name.
 *
 *  @param[in]     lst     The object name list.
 *  @param[in]     objects The object map to find object name.
 *  @param[in,out] out     The list to fill.
 */
template <typename T>
void parser::_get_objects_by_list_name(set_string const& lst,
                                       map_object& objects,
                                       std::list<T>& out) {
  for (set_string::const_iterator it(lst.begin()), end(lst.end()); it != end;
       ++it) {
    map_object::iterator it_obj(objects.find(*it));
    if (it_obj != objects.end())
      out.push_back(*static_cast<T*>(it_obj->second.get()));
  }
}

/**
 *  Insert objects into type T list and sort the new list by object id.
 *
 *  @param[in]  from The objects source.
 *  @param[out] to   The objects destination.
 */
template <typename T>
void parser::_insert(list_object const& from, std::set<T>& to) {
  for (list_object::const_iterator it(from.begin()), end(from.end()); it != end;
       ++it)
    to.insert(*static_cast<T const*>(it->get()));
}

/**
 *  Insert objects into type T list and sort the new list by object id.
 *
 *  @param[in]  from The objects source.
 *  @param[out] to   The objects destination.
 */
template <typename T>
void parser::_insert(map_object const& from, std::set<T>& to) {
  for (map_object::const_iterator it(from.begin()), end(from.end()); it != end;
       ++it)
    to.insert(*static_cast<T*>(it->second.get()));
}

/**
 *  Get the map object type name.
 *
 *  @param[in] objects  The map object.
 *
 *  @return The type name.
 */
std::string const& parser::_map_object_type(map_object const& objects) const
    throw() {
  static std::string const empty("");
  map_object::const_iterator it(objects.begin());
  if (it == objects.end())
    return empty;
  return it->second->type_name();
}

/**
 *  Parse the directory configuration.
 *
 *  @param[in] path The directory path.
 */
void parser::_parse_directory_configuration(std::string const& path) {
  for (const auto& entry : std::filesystem::directory_iterator(path)) {
    if (entry.is_regular_file() && entry.path().extension() == ".cfg")
      _parse_object_definitions(entry.path().string());
    else if (entry.is_directory())
      _parse_directory_configuration(entry.path().string());
  }
}

/**
 *  Parse the global configuration file.
 *
 *  @param[in] path The configuration path.
 */
void parser::_parse_global_configuration(const std::string& path) {
  _logger->info("Reading main configuration file '{}'.", path);

  std::ifstream stream(path.c_str(), std::ios::binary);
  if (!stream.is_open())
    throw msg_fmt(
        "Parsing of global configuration failed: Can't open file '{}'", path);

  _config->cfg_main(path);

  _current_line = 0;
  _current_path = path;

  std::string input;
  while (get_next_line(stream, input, _current_line)) {
    std::list<std::string> values =
        absl::StrSplit(input, absl::MaxSplits('=', 1));
    if (values.size() == 2) {
      auto it = values.begin();
      char const* key = it->c_str();
      ++it;
      char const* value = it->c_str();
      if (_config->set(key, value))
        continue;
    }
    throw msg_fmt(
        "Parsing of global configuration failed in file '{}' on line {}: "
        "Invalid line '{}'",
        path, _current_line, input);
  }
}

/**
 *  Parse the object definition file.
 *
 *  @param[in] path The object definitions path.
 */
void parser::_parse_object_definitions(std::string const& path) {
  _logger->info("Processing object config file '{}'", path);

  std::ifstream stream(path, std::ios::binary);
  if (!stream.is_open())
    throw msg_fmt("Parsing of object definition failed: Can't open file '{}'",
                  path);

  _current_line = 0;
  _current_path = path;

  bool parse_object = false;
  object_ptr obj;
  std::string input;
  while (get_next_line(stream, input, _current_line)) {
    // Multi-line.
    while ('\\' == input[input.size() - 1]) {
      input.resize(input.size() - 1);
      std::string addendum;
      if (!get_next_line(stream, addendum, _current_line))
        break;
      input.append(addendum);
    }

    // Check if is a valid object.
    if (obj == nullptr) {
      if (input.find("define") || !std::isspace(input[6]))
        throw msg_fmt(
            "Parsing of object definition failed in file '{}' on line {}: "
            "Unexpected start definition",
            _current_path, _current_line);
      input.erase(0, 6);
      absl::StripLeadingAsciiWhitespace(&input);
      std::size_t last = input.size() - 1;
      if (input.empty() || input[last] != '{')
        throw msg_fmt(
            "Parsing of object definition failed in file '{}' on line {}: "
            "Unexpected start definition",
            _current_path, _current_line);
      input.erase(last);
      absl::StripTrailingAsciiWhitespace(&input);
      obj = object::create(input);
      if (obj == nullptr)
        throw msg_fmt(
            "Parsing of object definition failed in file '{}' on line {}: "
            "Unknown object type name '{}'",
            _current_path, _current_line, input);
      parse_object = (_read_options & (1 << obj->type()));
      _objects_info[obj.get()] = file_info(path, _current_line);
    }
    // Check if is the not the end of the current object.
    else if (input != "}") {
      if (parse_object) {
        if (!obj->parse(input))
          throw msg_fmt(
              "Parsing of object definition failed in file '{}' on line {}: "
              "Invalid line '{}'",
              _current_path, _current_line, input);
      }
    }
    // End of the current object.
    else {
      if (parse_object) {
        if (!obj->name().empty())
          _add_template(obj);
        if (obj->should_register())
          _add_object(obj);
      }
      obj.reset();
    }
  }
}

/**
 *  Parse the resource file.
 *
 *  @param[in] path The resource file path.
 */
void parser::_parse_resource_file(std::string const& path) {
  _logger->info("Reading resource file '{}'", path);

  std::ifstream stream(path.c_str(), std::ios::binary);
  if (!stream.is_open())
    throw msg_fmt("Parsing of resource file failed: can't open file '{}'",
                  path);

  _current_line = 0;
  _current_path = path;

  std::string input;
  while (get_next_line(stream, input, _current_line)) {
    try {
      std::list<std::string> key_value =
          absl::StrSplit(input, absl::MaxSplits('=', 1));
      if (key_value.size() == 2) {
        auto it = key_value.begin();
        std::string& key = *it;
        ++it;
        std::string value = *it;
        _config->user(key, value);
      } else
        throw msg_fmt(
            "Parsing of resource file '{}' failed on line {}: Invalid line "
            "'{}'",
            _current_path, _current_line, input);
    } catch (std::exception const& e) {
      (void)e;
      throw msg_fmt(
          "Parsing of resource file '{}' failed on line {}: Invalid line '{}'",
          _current_path, _current_line, input);
    }
  }
}

/**
 *  Resolve template for register objects.
 */
void parser::_resolve_template(error_cnt& err) {
  for (map_object& templates : _templates) {
    for (map_object::iterator it = templates.begin(), end = templates.end();
         it != end; ++it)
      it->second->resolve_template(templates);
  }

  for (unsigned int i = 0; i < _lst_objects.size(); ++i) {
    map_object& templates = _templates[i];
    for (list_object::iterator it = _lst_objects[i].begin(),
                               end = _lst_objects[i].end();
         it != end; ++it) {
      (*it)->resolve_template(templates);
      try {
        (*it)->check_validity(err);
      } catch (std::exception const& e) {
        throw msg_fmt("Configuration parsing failed {}: {}",
                      _get_file_info(it->get()), e.what());
      }
    }
  }

  for (unsigned int i = 0; i < _map_objects.size(); ++i) {
    map_object& templates = _templates[i];
    for (map_object::iterator it = _map_objects[i].begin(),
                              end = _map_objects[i].end();
         it != end; ++it) {
      it->second->resolve_template(templates);
      try {
        it->second->check_validity(err);
      } catch (std::exception const& e) {
        throw msg_fmt("Configuration parsing failed {}: {}",
                      _get_file_info(it->second.get()), e.what());
      }
    }
  }
}

/**
 *  Store object into the list.
 *
 *  @param[in] obj The object to store.
 */
void parser::_store_into_list(object_ptr obj) {
  _lst_objects[obj->type()].push_back(obj);
}

/**
 *  Store object into the map.
 *
 *  @param[in] obj The object to store.
 */
template <typename T, std::string const& (T::*ptr)() const throw()>
void parser::_store_into_map(object_ptr obj) {
  std::shared_ptr<T> real(std::static_pointer_cast<T>(obj));
  map_object::iterator it(_map_objects[obj->type()].find((real.get()->*ptr)()));
  if (it != _map_objects[obj->type()].end())
    throw msg_fmt("Parsing of {} failed {}: {} already exists",
                  obj->type_name(), _get_file_info(obj.get()), obj->name());
  _map_objects[obj->type()][(real.get()->*ptr)()] = real;
}
