/*
** Copyright 2011-2014,2017 Centreon
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

#include "com/centreon/engine/configuration/parser.hh"
#include "com/centreon/engine/exceptions/error.hh"
#include "com/centreon/engine/log_v2.hh"
#include "com/centreon/engine/string.hh"
#include "com/centreon/io/directory_entry.hh"

using namespace com::centreon;
using namespace com::centreon::engine::configuration;
using namespace com::centreon::io;

using Descriptor = ::google::protobuf::Descriptor;
using FieldDescriptor = ::google::protobuf::FieldDescriptor;
using Message = ::google::protobuf::Message;
using Reflection = ::google::protobuf::Reflection;

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
 *  Default constructor.
 *
 *  @param[in] read_options Configuration file reading options
 *             (use to skip some object type).
 */
parser::parser(unsigned int read_options)
    : _config(NULL), _read_options(read_options) {}

/**
 *  Destructor.
 */
parser::~parser() noexcept {}

void parser::parse(const std::string& path, State* pb_config) {
  /* Parse the global configuration file. */
  _parse_global_configuration(path, pb_config);

  // parse configuration files.
  _apply(pb_config->cfg_file(), pb_config, &parser::_parse_object_definitions);
  // parse resource files.
  _apply(pb_config->resource_file(), pb_config, &parser::_parse_resource_file);
  // parse configuration directories.
  _apply(pb_config->cfg_dir(), &parser::_parse_directory_configuration);
}

/**
 *  Parse configuration file.
 *
 *  @param[in] path   The configuration file path.
 *  @param[in] config The state configuration to fill.
 */
void parser::parse(const std::string& path, state& config) {
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
  _resolve_template();

  // Apply extended info.
  _apply_hostextinfo();
  _apply_serviceextinfo();

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
  _insert(_lst_objects[object::service], config.services());
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
  return;
}

/**
 *  Add template into the list.
 *
 *  @param[in] obj The tempalte to add into the list.
 */
void parser::_add_template(object_ptr obj) {
  std::string const& name(obj->name());
  if (name.empty())
    throw engine_error() << "Parsing of " << obj->type_name() << " failed "
                         << _get_file_info(obj.get())
                         << ": Property 'name' "
                            "is missing";
  map_object& tmpl(_templates[obj->type()]);
  if (tmpl.find(name) != tmpl.end())
    throw engine_error() << "Parsing of " << obj->type_name() << " failed "
                         << _get_file_info(obj.get()) << ": " << name
                         << " already exists";
  tmpl[name] = obj;
}

/**
 *  Apply the host extended info.
 *
 *  @warning This function is for compatibility and has very
 *           poor performance. Didn't use extended info. If you
 *           want to use the generic template system.
 */
void parser::_apply_hostextinfo() {
  map_object& gl_hosts(_map_objects[object::host]);
  list_object const& hostextinfos(_lst_objects[object::hostextinfo]);
  for (list_object::const_iterator it(hostextinfos.begin()),
       end(hostextinfos.end());
       it != end; ++it) {
    // Get the current hostextinfo to check.
    hostextinfo_ptr obj(std::static_pointer_cast<hostextinfo>(*it));

    list_host hosts;
    _get_objects_by_list_name(obj->hosts(), gl_hosts, hosts);
    _get_hosts_by_hostgroups_name(obj->hostgroups(), hosts);

    for (list_host::iterator it(hosts.begin()), end(hosts.end()); it != end;
         ++it)
      it->merge(*obj);
  }
}

/**
 *  Apply the service extended info.
 *
 *  @warning This function is for compatibility and has very
 *           poor performance. Didn't use extended info. If you
 *           want to use the generic template system.
 */
void parser::_apply_serviceextinfo() {
  map_object& gl_hosts(_map_objects[object::host]);
  list_object& gl_services(_lst_objects[object::service]);
  list_object const& serviceextinfos(_lst_objects[object::serviceextinfo]);
  for (list_object::const_iterator it(serviceextinfos.begin()),
       end(serviceextinfos.end());
       it != end; ++it) {
    // Get the current serviceextinfo to check.
    serviceextinfo_ptr obj(std::static_pointer_cast<serviceextinfo>(*it));

    list_host hosts;
    _get_objects_by_list_name(obj->hosts(), gl_hosts, hosts);
    _get_hosts_by_hostgroups_name(obj->hostgroups(), hosts);

    for (list_object::iterator it(gl_services.begin()), end(gl_services.end());
         it != end; ++it) {
      service_ptr svc(std::static_pointer_cast<service>(*it));
      if (svc->service_description() != obj->service_description())
        continue;

      list_host svc_hosts;
      _get_objects_by_list_name(svc->hosts(), gl_hosts, svc_hosts);
      _get_hosts_by_hostgroups_name(svc->hostgroups(), svc_hosts);

      bool found(false);
      for (list_host::const_iterator it_host(hosts.begin()),
           end_host(hosts.end());
           !found && it_host != end_host; ++it_host) {
        for (list_host::const_iterator it_svc_host(svc_hosts.begin()),
             end_svc_host(svc_hosts.end());
             it_svc_host != end_svc_host; ++it_svc_host) {
          if (it_host->host_name() == it_svc_host->host_name()) {
            svc->merge(*obj);
            found = true;
          }
        }
      }
    }
  }
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
  throw engine_error() << "Parsing failed: Object not "
                          "found into the file information cache";
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
void parser::_parse_directory_configuration(const std::string& path,
                                            State* pb_config) {
  directory_entry dir(path);
  std::list<file_entry> const& lst(dir.entry_list("*.cfg"));
  for (std::list<file_entry>::const_iterator it(lst.begin()), end(lst.end());
       it != end; ++it)
    _parse_object_definitions(it->path());
}

/**
 *  Parse the directory configuration.
 *
 *  @param[in] path The directory path.
 */
void parser::_parse_directory_configuration(std::string const& path) {
  directory_entry dir(path);
  std::list<file_entry> const& lst(dir.entry_list("*.cfg"));
  for (std::list<file_entry>::const_iterator it(lst.begin()), end(lst.end());
       it != end; ++it)
    _parse_object_definitions(it->path());
}

template <typename T>
bool set(
    T* msg,
    const absl::string_view& key,
    const absl::string_view& value,
    const absl::flat_hash_map<std::string, std::string>& correspondance = {}) {
  const Descriptor* desc = msg->GetDescriptor();
  const FieldDescriptor* f =
      desc->FindFieldByName(std::string(key.data(), key.size()));
  if (f == nullptr) {
    auto it = correspondance.find(key);
    if (it != correspondance.end())
      f = desc->FindFieldByName(it->second);
    if (f == nullptr)
      return false;
  }
  const Reflection* refl = msg->GetReflection();
  switch (f->type()) {
    case FieldDescriptor::TYPE_BOOL: {
      bool val;
      if (absl::SimpleAtob(value, &val)) {
        refl->SetBool(static_cast<Message*>(msg), f, val);
        return true;
      } else
        return false;
    } break;
    case FieldDescriptor::TYPE_INT32: {
      int32_t val;
      if (absl::SimpleAtoi(value, &val)) {
        refl->SetInt32(static_cast<Message*>(msg), f, val);
        return true;
      } else
        return false;
    } break;
    case FieldDescriptor::TYPE_UINT32: {
      uint32_t val;
      if (absl::SimpleAtoi(value, &val)) {
        refl->SetUInt32(static_cast<Message*>(msg), f, val);
        return true;
      } else
        return false;
    } break;
    case FieldDescriptor::TYPE_UINT64: {
      uint64_t val;
      if (absl::SimpleAtoi(value, &val)) {
        refl->SetUInt64(static_cast<Message*>(msg), f, val);
        return true;
      } else
        return false;
    } break;
    case FieldDescriptor::TYPE_STRING:
      if (f->is_repeated()) {
        refl->AddString(static_cast<Message*>(msg), f,
                        std::string(value.data(), value.size()));
      } else {
        refl->SetString(static_cast<Message*>(msg), f,
                        std::string(value.data(), value.size()));
      }
      return true;
    default:
      return false;
  }
  return true;
}

template <typename T, typename U>
bool set(
    T* msg,
    const absl::string_view& key,
    const U&& value,
    const absl::flat_hash_map<std::string, std::string>& correspondance = {}) {
  const Descriptor* desc = msg->GetDescriptor();
  const FieldDescriptor* f =
      desc->FindFieldByName(std::string(key.data(), key.size()));
  if (f == nullptr) {
    auto it = correspondance.find(key);
    if (it != correspondance.end())
      f = desc->FindFieldByName(it->second);
    if (f == nullptr)
      return false;
  }
  const Reflection* refl = msg->GetReflection();
  if (f->is_repeated()) {
    auto* m = refl->AddMessage(static_cast<Message*>(msg), f);
    m->CopyFrom(std::move(value));
  } else
    refl->MutableMessage(static_cast<Message*>(msg), f)
        ->CopyFrom(std::move(value));
  return true;
}

void parser::_parse_global_configuration(const std::string& path,
                                         State* pb_config) {
  log_v2::config()->info("Reading main configuration file '{}'.", path);

  std::ifstream in(path, std::ios::in);
  std::string content;
  if (in) {
    in.seekg(0, std::ios::end);
    content.resize(in.tellg());
    in.seekg(0, std::ios::beg);
    in.read(&content[0], content.size());
    in.close();
  } else
    throw engine_error() << fmt::format(
        "Parsing of global configuration failed: can't open file '{}': {}",
        path, strerror(errno));

  pb_config->set_cfg_main(path);
  _current_line = 0;
  _current_path = path;

  auto tab{absl::StrSplit(content, '\n')};
  for (auto it = tab.begin(); it != tab.end(); ++it) {
    absl::string_view l = absl::StripAsciiWhitespace(*it);
    if (l.empty() || l[0] == '#')
      continue;
    std::pair<absl::string_view, absl::string_view> p = absl::StrSplit(l, '=');
    p.first = absl::StripTrailingAsciiWhitespace(p.first);
    p.second = absl::StripLeadingAsciiWhitespace(p.second);
    if (!set(pb_config, p.first, p.second))
      throw engine_error() << fmt::format(
          "Unable to parse '{}' key with value '{}'", p.first, p.second);
  }
}

/**
 * @brief Parse objects files (services.cfg, hosts.cfg, timeperiods.cfg...
 *
 * This function almost uses protobuf reflection to set values but it may fail
 * because of the syntax used in these files that can be a little different
 * from the message format.
 *
 * Two mechanisms are used to complete the reflection.
 * * A hastable <string, string> named correspondance is used in case of several
 *   keys to access to the same value. This is, for example, the case for
 *   host_id which is historically also named _HOST_ID.
 * * A std::function<bool(string_view_string_view) can also be defined in
 *   several cases to make special stuffs. For example, we use it for timeperiod
 *   object to set its timeranges.
 *
 * @param path The file to parse.
 * @param pb_config The configuration to complete.
 */
void parser::_parse_object_definitions(const std::string& path,
                                       State* pb_config) {
  log_v2::config()->info("Processing object config file '{}'", path);

  std::ifstream in(path, std::ios::in);
  std::string content;
  if (in) {
    in.seekg(0, std::ios::end);
    content.resize(in.tellg());
    in.seekg(0, std::ios::beg);
    in.read(&content[0], content.size());
    in.close();
  } else
    throw engine_error() << fmt::format(
        "Parsing of object definition failed: can't open file '{}': {}", path,
        strerror(errno));

  auto tab{absl::StrSplit(content, '\n')};
  std::string ll;
  bool append_to_previous_line = false;
  Message* msg = nullptr;
  int current_line = 1;
  std::string type;
  absl::flat_hash_map<std::string, std::string> correspondance;
  std::function<bool(const absl::string_view& key,
                     const absl::string_view& value)>
      hook;

  for (auto it = tab.begin(); it != tab.end(); ++it, current_line++) {
    absl::string_view l = absl::StripAsciiWhitespace(*it);
    if (l.empty() || l[0] == '#')
      continue;

    /* Multiline */
    if (append_to_previous_line) {
      if (l[l.size() - 1] == '\\') {
        ll.append(l.data(), l.size() - 1);
        continue;
      } else {
        ll.append(l.data(), l.size());
        append_to_previous_line = false;
        l = ll;
      }
    } else if (l[l.size() - 1] == '\\') {
      ll = std::string(l.data(), l.size() - 1);
      append_to_previous_line = true;
      continue;
    }

    if (msg) {
      if (l.empty())
        continue;
      /* is it time to close the definition? */
      if (l == "}") {
        if (type == "contact") {
          auto* cts = pb_config->mutable_contacts();
          Contact* ct = static_cast<Contact*>(msg);
          (*cts)[ct->name()] = std::move(*ct);
          delete msg;
          msg = nullptr;
        } else {
          msg = nullptr;
          hook = nullptr;
        }
      } else {
        /* Main part where keys/values are read */
        /* ------------------------------------ */
        size_t pos = l.find_first_of(" \t");
        absl::string_view key = l.substr(0, pos);
        l.remove_prefix(pos);
        l = absl::StripLeadingAsciiWhitespace(l);
        /* Classical part */
        if (!set(msg, key, l, correspondance)) {
          bool retval = false;
          /* particular cases with hook */
          if (hook)
            retval = hook(key, l);
          if (!retval) {
            /* last particular case with customvariables */
            CustomVariable cv;
            key.remove_prefix(1);
            cv.set_name(key.data(), key.size());
            cv.set_value(l.data(), l.size());

            const Descriptor* desc = msg->GetDescriptor();
            const FieldDescriptor* f = desc->FindFieldByName("customvariables");
            if (f == nullptr)
              throw engine_error() << fmt::format(
                  "Unable to parse '{}' key with value '{}' in message of type "
                  "'{}'",
                  key, l, type);
            else {
              const Reflection* refl = msg->GetReflection();
              CustomVariable* cv =
                  static_cast<CustomVariable*>(refl->AddMessage(msg, f));
              cv->set_name(key.data(), key.size());
              cv->set_value(l.data(), l.size());
            }
          }
        }
      }
    } else {
      if (!absl::StartsWith(l, "define") || !std::isspace(l[6]))
        throw engine_error() << fmt::format(
            "Parsing of object definition failed in file '{}' at line {}: "
            "Unexpected start definition",
            path, current_line);
      /* Let's remove the first 6 characters ("define") */
      l = absl::StripLeadingAsciiWhitespace(l.substr(6));
      if (l.empty() || l[l.size() - 1] != '{')
        throw engine_error() << fmt::format(
            "Parsing of object definition failed in file '{}' at line {}; "
            "unexpected start definition",
            path, current_line);
      l = absl::StripTrailingAsciiWhitespace(l.substr(0, l.size() - 1));
      type = std::string(l.data(), l.size());
      if (type == "contact")
        msg = new Contact;
      else if (type == "host") {
        msg = pb_config->mutable_hosts()->Add();
        correspondance = {
            {"_HOST_ID", "host_id"},
        };
      } else if (type == "service") {
        msg = pb_config->mutable_services()->Add();
        correspondance = {
            {"_SERVICE_ID", "service_id"},
        };
      } else if (type == "timeperiod") {
        msg = pb_config->mutable_timeperiods()->Add();
        correspondance = {
            {"name", "timeperiod_name"},
        };
        hook = [msg, tp = static_cast<Timeperiod*>(msg)](
                   const absl::string_view& key,
                   const absl::string_view& value) -> bool {
          auto arr = absl::StrSplit(value, ',');
          for (auto& d : arr) {
            std::pair<absl::string_view, absl::string_view> v =
                absl::StrSplit(d, '-');
            TimeRange tr;
            std::pair<absl::string_view, absl::string_view> p =
                absl::StrSplit(v.first, ':');
            uint32_t h, m;
            if (!absl::SimpleAtoi(p.first, &h) ||
                !absl::SimpleAtoi(p.second, &m))
              return false;
            tr.set_range_start(h * 3600 + m * 60);
            p = absl::StrSplit(v.second, ':');
            if (!absl::SimpleAtoi(p.first, &h) ||
                !absl::SimpleAtoi(p.second, &m))
              return false;
            tr.set_range_end(h * 3600 + m * 60);
            set(msg, key, std::move(tr));
          }
          return true;
        };
      } else if (type == "command") {
        msg = pb_config->mutable_commands()->Add();
      } else {
        assert(1 == 18);
      }
    }
  }
}

void parser::_parse_resource_file(const std::string& path, State* pb_config) {
  log_v2::config()->info("Reading resource file '{}'", path);

  std::ifstream in(path, std::ios::in);
  std::string content;
  if (in) {
    in.seekg(0, std::ios::end);
    content.resize(in.tellg());
    in.seekg(0, std::ios::beg);
    in.read(&content[0], content.size());
    in.close();
  } else
    throw engine_error() << fmt::format(
        "Parsing of resource file failed: can't open file '{}': {}", path,
        strerror(errno));

  auto tab{absl::StrSplit(content, '\n')};
  int current_line = 1;
  for (auto it = tab.begin(); it != tab.end(); ++it, current_line++) {
    absl::string_view l = absl::StripLeadingAsciiWhitespace(*it);
    if (l.empty() || l[0] == '#')
      continue;
    std::pair<absl::string_view, absl::string_view> p = absl::StrSplit(l, '=');
    p.first = absl::StripTrailingAsciiWhitespace(p.first);
    p.second = absl::StripLeadingAsciiWhitespace(p.second);
    if (p.first.size() >= 3 && p.first[0] == '$' &&
        p.first[p.first.size() - 1] == '$') {
      p.first = p.first.substr(1, p.first.size() - 2);
      (*pb_config
            ->mutable_users())[std::string(p.first.data(), p.first.size())] =
          std::string(p.second.data(), p.second.size());
    } else
      throw engine_error() << fmt::format("Invalid user key '{}'", p.first);
  }
}

/**
 *  Parse the global configuration file.
 *
 *  @param[in] path The configuration path.
 */
void parser::_parse_global_configuration(const std::string& path) {
  engine_logger(logging::log_info_message, logging::most)
      << "Reading main configuration file '" << path << "'.";
  log_v2::config()->info("Reading main configuration file '{}'.", path);

  std::ifstream stream(path, std::ios::binary);
  if (!stream.is_open())
    throw engine_error()
        << "Parsing of global configuration failed: can't open file '" << path
        << "'";

  _config->cfg_main(path);

  _current_line = 0;
  _current_path = path;

  std::string input;
  while (string::get_next_line(stream, input, _current_line)) {
    char const* key;
    char const* value;
    if (!string::split(input, &key, &value, '=') || !_config->set(key, value))
      throw engine_error() << "Parsing of global "
                              "configuration failed in file '"
                           << path << "' on line " << _current_line
                           << ": Invalid line '" << input << "'";
  }
}

/**
 *  Parse the object definition file.
 *
 *  @param[in] path The object definitions path.
 */
void parser::_parse_object_definitions(std::string const& path) {
  engine_logger(logging::log_info_message, logging::basic)
      << "Processing object config file '" << path << "'";
  log_v2::config()->info("Processing object config file '{}'", path);

  std::ifstream stream(path, std::ios::binary);
  if (!stream.is_open())
    throw engine_error() << "Parsing of object definition failed: "
                         << "can't open file '" << path << "'";

  _current_line = 0;
  _current_path = path;

  bool parse_object = false;
  object_ptr obj;
  std::string input;
  while (string::get_next_line(stream, input, _current_line)) {
    // Multi-line.
    while ('\\' == input[input.size() - 1]) {
      input.resize(input.size() - 1);
      std::string addendum;
      if (!string::get_next_line(stream, addendum, _current_line))
        break;
      input.append(addendum);
    }

    // Check if is a valid object.
    if (obj == nullptr) {
      if (input.find("define") || !std::isspace(input[6]))
        throw engine_error()
            << "Parsing of object definition failed "
            << "in file '" << _current_path << "' on line " << _current_line
            << ": Unexpected start definition";
      string::trim_left(input.erase(0, 6));
      std::size_t last(input.size() - 1);
      if (input.empty() || input[last] != '{')
        throw engine_error()
            << "Parsing of object definition failed "
            << "in file '" << _current_path << "' on line " << _current_line
            << ": Unexpected start definition";
      std::string const& type(string::trim_right(input.erase(last)));
      obj = object::create(type);
      if (obj == nullptr)
        throw engine_error()
            << "Parsing of object definition failed "
            << "in file '" << _current_path << "' on line " << _current_line
            << ": Unknown object type name '" << type << "'";
      parse_object = (_read_options & (1 << obj->type()));
      _objects_info[obj.get()] = file_info(path, _current_line);
    }
    // Check if is the not the end of the current object.
    else if (input != "}") {
      if (parse_object) {
        if (!obj->parse(input))
          throw engine_error()
              << "Parsing of object definition "
              << "failed in file '" << _current_path << "' on line "
              << _current_line << ": Invalid line '" << input << "'";
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
  engine_logger(logging::log_info_message, logging::most)
      << "Reading resource file '" << path << "'";
  log_v2::config()->info("Reading resource file '{}'", path);

  std::ifstream stream(path.c_str(), std::ios::binary);
  if (!stream.is_open())
    throw engine_error() << "Parsing of resource file failed: "
                         << "can't open file '" << path << "'";

  _current_line = 0;
  _current_path = path;

  std::string input;
  while (string::get_next_line(stream, input, _current_line)) {
    try {
      std::string key;
      std::string value;
      if (!string::split(input, key, value, '='))
        throw engine_error() << "Parsing of resource file '" << _current_path
                             << "' failed on line " << _current_line
                             << ": Invalid line '" << input << "'";
      _config->user(key, value);
    } catch (std::exception const& e) {
      (void)e;
      throw engine_error() << "Parsing of resource file '" << _current_path
                           << "' failed on line " << _current_line
                           << ": Invalid line '" << input << "'";
    }
  }
}

/**
 *  Resolve template for register objects.
 */
void parser::_resolve_template() {
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
        (*it)->check_validity();
      } catch (std::exception const& e) {
        throw engine_error() << "Configuration parsing failed "
                             << _get_file_info(it->get()) << ": " << e.what();
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
        it->second->check_validity();
      } catch (std::exception const& e) {
        throw engine_error()
            << "Configuration parsing failed "
            << _get_file_info(it->second.get()) << ": " << e.what();
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
    throw engine_error() << "Parsing of " << obj->type_name() << " failed "
                         << _get_file_info(obj.get()) << ": " << obj->name()
                         << " alrealdy exists";
  _map_objects[obj->type()][(real.get()->*ptr)()] = real;
}
