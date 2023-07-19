/*
 * Copyright 2011-2014,2017,2022-2023 Centreon
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

#include "com/centreon/engine/configuration/parser.hh"
#include <absl/container/flat_hash_set.h>
#include <memory>
#include "absl/strings/numbers.h"
#include "com/centreon/engine/exceptions/error.hh"
#include "com/centreon/engine/log_v2.hh"
#include "com/centreon/engine/string.hh"
#include "com/centreon/io/directory_entry.hh"
#include "configuration/anomalydetection_helper.hh"
#include "configuration/command_helper.hh"
#include "configuration/connector_helper.hh"
#include "configuration/contact_helper.hh"
#include "configuration/contactgroup_helper.hh"
#include "configuration/host_helper.hh"
#include "configuration/hostdependency_helper.hh"
#include "configuration/hostescalation_helper.hh"
#include "configuration/hostgroup_helper.hh"
#include "configuration/message_helper.hh"
#include "configuration/service_helper.hh"
#include "configuration/servicedependency_helper.hh"
#include "configuration/serviceescalation_helper.hh"
#include "configuration/servicegroup_helper.hh"
#include "configuration/severity_helper.hh"
#include "configuration/state_helper.hh"
#include "configuration/tag_helper.hh"
#include "configuration/timeperiod_helper.hh"

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

void parser::parse(const std::string& path, State* pb_config) {
  /* Parse the global configuration file. */
  auto helper = std::make_unique<state_helper>(pb_config);
  _pb_helper[pb_config] = std::move(helper);
  _parse_global_configuration(path, pb_config);

  // parse configuration files.
  _apply(pb_config->cfg_file(), pb_config, &parser::_parse_object_definitions);
  // parse resource files.
  _apply(pb_config->resource_file(), pb_config, &parser::_parse_resource_file);
  // parse configuration directories.
  _apply(pb_config->cfg_dir(), pb_config,
         &parser::_parse_directory_configuration);

  // Apply template.
  _resolve_template(pb_config);

  _cleanup(pb_config);
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
  const std::list<file_entry>& lst(dir.entry_list("*.cfg"));
  for (auto& fe : lst)
    _parse_object_definitions(fe.path(), pb_config);
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

bool set_global(std::unique_ptr<message_helper>& helper,
                const absl::string_view& key,
                const absl::string_view& value) {
  //    const absl::flat_hash_map<std::string, std::string>& correspondence =
  //    {}) {
  State* msg = static_cast<State*>(helper->mut_obj());
  const Descriptor* desc = msg->GetDescriptor();
  const FieldDescriptor* f;
  const Reflection* refl;

  f = desc->FindFieldByName(std::string(key.data(), key.size()));
  if (f == nullptr) {
    auto it = helper->correspondence().find(key);
    if (it != helper->correspondence().end())
      f = desc->FindFieldByName(it->second);
    if (f == nullptr)
      return false;
  }
  refl = msg->GetReflection();
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
    case FieldDescriptor::TYPE_FLOAT: {
      float val;
      if (absl::SimpleAtof(value, &val)) {
        refl->SetFloat(static_cast<Message*>(msg), f, val);
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
    case FieldDescriptor::TYPE_MESSAGE:
      if (!f->is_repeated()) {
        Message* m = refl->MutableMessage(msg, f);
        const Descriptor* d = m->GetDescriptor();

        if (d && d->name() == "StringSet") {
          StringSet* set =
              static_cast<StringSet*>(refl->MutableMessage(msg, f));
          fill_string_group(set, value);
          return true;
        } else if (d && d->name() == "StringList") {
          StringList* lst =
              static_cast<StringList*>(refl->MutableMessage(msg, f));
          fill_string_group(lst, value);
          return true;
        }
      }
    default:
      return false;
  }
  return true;
}

/**
 * @brief Set the value given as a string to the object key. If the key does
 * not exist, the correspondence table may be used to find a replacement of
 * the key. The function converts the value to the appropriate type.
 *
 * Another important point is that many configuration objects contain the Object
 * obj message (something like an inheritance). This message contains three
 * fields name, use and register that are important for templating. If keys are
 * one of these names, the function tries to work directly with the obj message.
 *
 * @tparam T The type of the message containing the object key.
 * @param msg The message containing the object key.
 * @param key The key to localize the object to set.
 * @param value The value as string that will be converted to the good type.
 * @param correspondence A hash table giving traductions from keys to others.
 * If a key fails, correspondence is used to find a new replacement key.
 *
 * @return true on success.
 */
bool set(std::unique_ptr<message_helper>& helper,
         const absl::string_view& key,
         const absl::string_view& value) {
  Message* msg = helper->mut_obj();
  const Descriptor* desc = msg->GetDescriptor();
  const FieldDescriptor* f;
  const Reflection* refl;

  /* Cases  where we have to work on the obj Object (the parent object) */
  if (key == "name" || key == "register" || key == "use") {
    f = desc->FindFieldByName("obj");
    if (f) {
      refl = msg->GetReflection();
      Object* obj = static_cast<Object*>(refl->MutableMessage(msg, f));

      /* Optimization to avoid a new string comparaison */
      switch (key[0]) {
        case 'n':  // name
          obj->set_name(std::string(value.data(), value.size()));
          break;
        case 'r': {  // register
          bool value_b;
          if (!absl::SimpleAtob(value, &value_b))
            return false;
          else
            obj->set_register_(value_b);
        } break;
        case 'u': {  // use
          obj->mutable_use()->Clear();
          auto arr = absl::StrSplit(value, ',');
          for (auto& t : arr) {
            std::string v{absl::StripAsciiWhitespace(t)};
            obj->mutable_use()->Add(std::move(v));
          }
        } break;
      }
      return true;
    }
  }

  f = desc->FindFieldByName(std::string(key.data(), key.size()));
  if (f == nullptr) {
    auto it = helper->correspondence().find(key);
    if (it != helper->correspondence().end())
      f = desc->FindFieldByName(it->second);
    if (f == nullptr)
      return false;
  }
  refl = msg->GetReflection();
  switch (f->type()) {
    case FieldDescriptor::TYPE_BOOL: {
      bool val;
      if (absl::SimpleAtob(value, &val)) {
        refl->SetBool(static_cast<Message*>(msg), f, val);
        helper->set_changed(f->number());
        return true;
      } else
        return false;
    } break;
    case FieldDescriptor::TYPE_INT32: {
      int32_t val;
      if (absl::SimpleAtoi(value, &val)) {
        refl->SetInt32(static_cast<Message*>(msg), f, val);
        helper->set_changed(f->number());
        return true;
      } else
        return false;
    } break;
    case FieldDescriptor::TYPE_UINT32: {
      uint32_t val;
      if (absl::SimpleAtoi(value, &val)) {
        refl->SetUInt32(static_cast<Message*>(msg), f, val);
        helper->set_changed(f->number());
        return true;
      } else
        return false;
    } break;
    case FieldDescriptor::TYPE_UINT64: {
      uint64_t val;
      if (absl::SimpleAtoi(value, &val)) {
        refl->SetUInt64(static_cast<Message*>(msg), f, val);
        helper->set_changed(f->number());
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
      helper->set_changed(f->number());
      return true;
    case FieldDescriptor::TYPE_MESSAGE:
      if (!f->is_repeated()) {
        Message* m = refl->MutableMessage(msg, f);
        const Descriptor* d = m->GetDescriptor();

        if (d && d->name() == "StringSet") {
          StringSet* set =
              static_cast<StringSet*>(refl->MutableMessage(msg, f));
          fill_string_group(set, value);
          helper->set_changed(f->number());
          return true;
        } else if (d && d->name() == "StringList") {
          StringList* lst =
              static_cast<StringList*>(refl->MutableMessage(msg, f));
          fill_string_group(lst, value);
          helper->set_changed(f->number());
          return true;
        }
      }
    default:
      return false;
  }
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
  auto& cfg_helper = _pb_helper[pb_config];
  for (auto it = tab.begin(); it != tab.end(); ++it) {
    absl::string_view l = absl::StripAsciiWhitespace(*it);
    if (l.empty() || l[0] == '#')
      continue;
    std::pair<absl::string_view, absl::string_view> p = absl::StrSplit(l, '=');
    p.first = absl::StripTrailingAsciiWhitespace(p.first);
    p.second = absl::StripLeadingAsciiWhitespace(p.second);
    bool retval = false;
    /* particular cases with hook */
    retval = cfg_helper->hook(p.first, p.second);
    if (!retval) {
      if (!set_global(cfg_helper, p.first, p.second))
        log_v2::config()->error("Unable to parse '{}' key with value '{}'",
                                p.first, p.second);
    }
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
 * * A hastable <string, string> named correspondence is used in case of several
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
  std::unique_ptr<Message> msg;
  std::unique_ptr<message_helper> msg_helper;

  int current_line = 1;
  std::string type;

  for (auto it = tab.begin(); it != tab.end(); ++it, current_line++) {
    absl::string_view l = absl::StripAsciiWhitespace(*it);
    if (l.empty() || l[0] == '#' || l[0] == ';')
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
        const Descriptor* desc = msg->GetDescriptor();
        const FieldDescriptor* f = desc->FindFieldByName("obj");
        const Reflection* refl = msg->GetReflection();
        if (f) {
          const Object& obj =
              *static_cast<const Object*>(&refl->GetMessage(*msg, f));
          auto otype = msg_helper->otype();
          _pb_helper[msg.get()] = std::move(msg_helper);
          if (!obj.name().empty()) {
            pb_map_object& tmpl = _pb_templates[otype];
            auto it = tmpl.find(obj.name());
            if (it != tmpl.end())
              throw engine_error() << fmt::format(
                  "Parsing of '{}' failed {}: {} already exists", type,
                  "file_info" /*_get_file_info(obj.get()) */, obj.name());
            if (!obj.register_())
              tmpl[obj.name()] = std::move(msg);
            else {
              auto copy = std::unique_ptr<Message>(msg->New());
              copy->CopyFrom(*msg);
              _pb_helper[copy.get()] =
                  message_helper::clone(*_pb_helper[msg.get()], copy.get());
              tmpl[obj.name()] = std::move(copy);
            }
          }
          if (obj.register_()) {
            switch (otype) {
              case message_helper::contact:
                pb_config->mutable_contacts()->AddAllocated(
                    static_cast<Contact*>(msg.release()));
                break;
              case message_helper::host:
                pb_config->mutable_hosts()->AddAllocated(
                    static_cast<Host*>(msg.release()));
                break;
              case message_helper::service:
                pb_config->mutable_services()->AddAllocated(
                    static_cast<Service*>(msg.release()));
                break;
              case message_helper::anomalydetection:
                pb_config->mutable_anomalydetections()->AddAllocated(
                    static_cast<Anomalydetection*>(msg.release()));
                break;
              case message_helper::hostdependency:
                pb_config->mutable_hostdependencies()->AddAllocated(
                    static_cast<Hostdependency*>(msg.release()));
                break;
              case message_helper::servicedependency:
                pb_config->mutable_servicedependencies()->AddAllocated(
                    static_cast<Servicedependency*>(msg.release()));
                break;
              case message_helper::timeperiod:
                pb_config->mutable_timeperiods()->AddAllocated(
                    static_cast<Timeperiod*>(msg.release()));
                break;
              case message_helper::command:
                pb_config->mutable_commands()->AddAllocated(
                    static_cast<Command*>(msg.release()));
                break;
              case message_helper::hostgroup:
                pb_config->mutable_hostgroups()->AddAllocated(
                    static_cast<Hostgroup*>(msg.release()));
                break;
              case message_helper::servicegroup:
                pb_config->mutable_servicegroups()->AddAllocated(
                    static_cast<Servicegroup*>(msg.release()));
                break;
              case message_helper::tag:
                pb_config->mutable_tags()->AddAllocated(
                    static_cast<Tag*>(msg.release()));
                break;
              case message_helper::contactgroup:
                pb_config->mutable_contactgroups()->AddAllocated(
                    static_cast<Contactgroup*>(msg.release()));
                break;
              case message_helper::connector:
                pb_config->mutable_connectors()->AddAllocated(
                    static_cast<Connector*>(msg.release()));
                break;
              case message_helper::severity:
                pb_config->mutable_severities()->AddAllocated(
                    static_cast<Severity*>(msg.release()));
                break;
              case message_helper::serviceescalation:
                pb_config->mutable_serviceescalations()->AddAllocated(
                    static_cast<Serviceescalation*>(msg.release()));
                break;
              case message_helper::hostescalation:
                pb_config->mutable_hostescalations()->AddAllocated(
                    static_cast<Hostescalation*>(msg.release()));
                break;
              default:
                log_v2::config()->critical(
                    "Attempt to add an object of unknown type");
            }
          }
        }
        msg = nullptr;
      } else {
        /* Main part where keys/values are read */
        /* ------------------------------------ */
        size_t pos = l.find_first_of(" \t");
        absl::string_view key = l.substr(0, pos);
        if (pos != std::string::npos) {
          l.remove_prefix(pos);
          l = absl::StripLeadingAsciiWhitespace(l);
        } else
          l = {};

        bool retval = false;
        /* particular cases with hook */
        retval = msg_helper->hook(key, l);

        if (!retval) {
          /* Classical part */
          if (!set(msg_helper, key, l)) {
            if (!msg_helper->insert_customvariable(key, l))
              throw engine_error() << fmt::format(
                  "Unable to parse '{}' key with value '{}' in message of type "
                  "'{}'",
                  key, l, type);
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
      if (type == "contact") {
        msg = std::make_unique<Contact>();
        msg_helper =
            std::make_unique<contact_helper>(static_cast<Contact*>(msg.get()));
      } else if (type == "host") {
        msg = std::make_unique<Host>();
        msg_helper =
            std::make_unique<host_helper>(static_cast<Host*>(msg.get()));
      } else if (type == "service") {
        msg = std::make_unique<Service>();
        msg_helper =
            std::make_unique<service_helper>(static_cast<Service*>(msg.get()));
      } else if (type == "anomalydetection") {
        msg = std::make_unique<Anomalydetection>();
        msg_helper = std::make_unique<anomalydetection_helper>(
            static_cast<Anomalydetection*>(msg.get()));
      } else if (type == "hostdependency") {
        msg = std::make_unique<Hostdependency>();
        msg_helper = std::make_unique<hostdependency_helper>(
            static_cast<Hostdependency*>(msg.get()));
      } else if (type == "servicedependency") {
        msg = std::make_unique<Servicedependency>();
        msg_helper = std::make_unique<servicedependency_helper>(
            static_cast<Servicedependency*>(msg.get()));
      } else if (type == "timeperiod") {
        msg = std::make_unique<Timeperiod>();
        msg_helper = std::make_unique<timeperiod_helper>(
            static_cast<Timeperiod*>(msg.get()));
      } else if (type == "command") {
        msg = std::make_unique<Command>();
        msg_helper =
            std::make_unique<command_helper>(static_cast<Command*>(msg.get()));
      } else if (type == "hostgroup") {
        msg = std::make_unique<Hostgroup>();
        msg_helper = std::make_unique<hostgroup_helper>(
            static_cast<Hostgroup*>(msg.get()));
      } else if (type == "servicegroup") {
        msg = std::make_unique<Servicegroup>();
        msg_helper = std::make_unique<servicegroup_helper>(
            static_cast<Servicegroup*>(msg.get()));
      } else if (type == "tag") {
        msg = std::make_unique<Tag>();
        msg_helper = std::make_unique<tag_helper>(static_cast<Tag*>(msg.get()));
      } else if (type == "contactgroup") {
        msg = std::make_unique<Contactgroup>();
        msg_helper = std::make_unique<contactgroup_helper>(
            static_cast<Contactgroup*>(msg.get()));
      } else if (type == "connector") {
        msg = std::make_unique<Connector>();
        msg_helper = std::make_unique<connector_helper>(
            static_cast<Connector*>(msg.get()));
      } else if (type == "severity") {
        msg = std::make_unique<Severity>();
        msg_helper = std::make_unique<severity_helper>(
            static_cast<Severity*>(msg.get()));
      } else if (type == "serviceescalation") {
        msg = std::make_unique<Serviceescalation>();
        msg_helper = std::make_unique<serviceescalation_helper>(
            static_cast<Serviceescalation*>(msg.get()));
      } else if (type == "hostescalation") {
        msg = std::make_unique<Hostescalation>();
        msg_helper = std::make_unique<hostescalation_helper>(
            static_cast<Hostescalation*>(msg.get()));
      } else {
        log_v2::config()->error("Type '{}' not yet supported by the parser",
                                type);
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
    if (l.empty() || l[0] == '#' || l[0] == ';')
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
      _objects_info.emplace(obj.get(), file_info(path, _current_line));
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

void parser::_merge(std::unique_ptr<message_helper>& msg_helper,
                    Message* tmpl) {
  Message* msg = msg_helper->mut_obj();
  const Descriptor* desc = msg->GetDescriptor();
  const Reflection* refl = msg->GetReflection();
  std::string tmp_str;

  for (int i = 0; i < desc->field_count(); ++i) {
    const FieldDescriptor* f = desc->field(i);
    if (f->name() != "obj") {
      /* Optional? If not defined in template, we continue. */
      const auto* oof = f->containing_oneof();
      if (oof) {
        if (!refl->GetOneofFieldDescriptor(*tmpl, oof))
          continue;
      }

      if ((oof && !refl->GetOneofFieldDescriptor(*msg, oof)) ||
          !msg_helper->changed(f->number())) {
        if (f->is_repeated()) {
          switch (f->cpp_type()) {
            case FieldDescriptor::CPPTYPE_STRING: {
              size_t count = refl->FieldSize(*tmpl, f);
              for (size_t j = 0; j < count; ++j) {
                const std::string& s =
                    refl->GetRepeatedStringReference(*tmpl, f, j, &tmp_str);
                size_t count_msg = refl->FieldSize(*msg, f);
                std::string tmp_str1;
                bool found = false;
                for (size_t k = 0; k < count_msg; ++k) {
                  const std::string& s1 =
                      refl->GetRepeatedStringReference(*msg, f, k, &tmp_str1);
                  if (s1 == s) {
                    found = true;
                    break;
                  }
                }
                if (!found)
                  refl->AddString(msg, f, s);
              }
            } break;
            case FieldDescriptor::CPPTYPE_MESSAGE: {
              size_t count = refl->FieldSize(*tmpl, f);
              for (size_t j = 0; j < count; ++j) {
                const Message& m = refl->GetRepeatedMessage(*tmpl, f, j);
                const Descriptor* d = m.GetDescriptor();
                size_t count_msg = refl->FieldSize(*msg, f);
                bool found = false;
                for (size_t k = 0; k < count_msg; ++k) {
                  const Message& m1 = refl->GetRepeatedMessage(*msg, f, k);
                  const Descriptor* d1 = m1.GetDescriptor();
                  if (d && d1 && d->name() == "PairUint64_32" &&
                      d1->name() == "PairUint64_32") {
                    const PairUint64_32& p =
                        static_cast<const PairUint64_32&>(m);
                    const PairUint64_32& p1 =
                        static_cast<const PairUint64_32&>(m1);
                    if (p.first() == p1.first() && p.second() == p1.second()) {
                      found = true;
                      break;
                    }
                  }
                }
                if (!found) {
                  Message* new_m = refl->AddMessage(msg, f);
                  new_m->CopyFrom(m);
                }
              }
            } break;
            default:
              log_v2::config()->error(
                  "Repeated type f->cpp_type = {} not managed in the "
                  "inheritence.",
                  f->cpp_type());
              assert(124 == 294);
          }
        } else {
          switch (f->cpp_type()) {
            case FieldDescriptor::CPPTYPE_STRING:
              refl->SetString(msg, f, refl->GetString(*tmpl, f));
              break;
            case FieldDescriptor::CPPTYPE_BOOL:
              refl->SetBool(msg, f, refl->GetBool(*tmpl, f));
              break;
            case FieldDescriptor::CPPTYPE_INT32:
              refl->SetInt32(msg, f, refl->GetInt32(*tmpl, f));
              break;
            case FieldDescriptor::CPPTYPE_UINT32:
              refl->SetUInt32(msg, f, refl->GetUInt32(*tmpl, f));
              break;
            case FieldDescriptor::CPPTYPE_UINT64:
              refl->SetUInt64(msg, f, refl->GetUInt64(*tmpl, f));
              break;
            case FieldDescriptor::CPPTYPE_MESSAGE: {
              Message* m = refl->MutableMessage(msg, f);
              const Descriptor* d = m->GetDescriptor();

              if (d && d->name() == "StringSet") {
                StringSet* orig_set =
                    static_cast<StringSet*>(refl->MutableMessage(tmpl, f));
                StringSet* set =
                    static_cast<StringSet*>(refl->MutableMessage(msg, f));
                if (set->additive()) {
                  for (auto& v : orig_set->data()) {
                    bool found = false;
                    for (auto& s : *set->mutable_data()) {
                      if (s == v) {
                        found = true;
                        break;
                      }
                    }
                    if (!found)
                      set->add_data(v);
                  }
                } else if (set->data().empty())
                  *set->mutable_data() = orig_set->data();

              } else if (d && d->name() == "StringList") {
                StringList* orig_lst =
                    static_cast<StringList*>(refl->MutableMessage(tmpl, f));
                StringList* lst =
                    static_cast<StringList*>(refl->MutableMessage(msg, f));
                if (lst->additive()) {
                  for (auto& v : orig_lst->data())
                    lst->add_data(v);
                } else if (lst->data().empty())
                  *lst->mutable_data() = orig_lst->data();
              }
            } break;

            default:
              log_v2::config()->error(
                  "Entry '{}' of type {} not managed in merge", f->name(),
                  f->type_name());
              assert(123 == 293);
          }
        }
      }
    }
  }
}

void parser::_resolve_template(std::unique_ptr<message_helper>& msg_helper,
                               const pb_map_object& tmpls) {
  if (msg_helper->resolved())
    return;
  Message* msg = msg_helper->mut_obj();

  msg_helper->resolve();
  const Descriptor* desc = msg->GetDescriptor();
  const FieldDescriptor* f = desc->FindFieldByName("obj");
  const Reflection* refl = msg->GetReflection();
  if (!f)
    return;

  Object* obj = static_cast<Object*>(refl->MutableMessage(msg, f));
  for (const std::string& u : obj->use()) {
    auto it = tmpls.find(u);
    if (it == tmpls.end())
      throw engine_error() << "Cannot merge object of type '" << u << "'";
    _resolve_template(_pb_helper[it->second.get()], tmpls);
    _merge(msg_helper, it->second.get());
  }
}

/**
 * @brief Return true if the register flag is enabled in the configuration
 * object.
 *
 * @param msg A configuration object as Protobuf message.
 *
 * @return True if it has to be registered, false otherwise.
 */
bool parser::_is_registered(const Message& msg) const {
  const Descriptor* desc = msg.GetDescriptor();
  const Reflection* refl = msg.GetReflection();
  std::string tmpl;
  const FieldDescriptor* f = desc->FindFieldByName("obj");
  if (f) {
    const Object& obj = static_cast<const Object&>(refl->GetMessage(msg, f));
    return obj.register_();
  }
  return false;
}

/**
 * @brief For each type of object in the State, templates are resolved that is
 * to say, children inherite from parents properties.
 *
 * @param pb_config The State containing all the object to handle.
 */
void parser::_resolve_template(State* pb_config) {
  for (Command& c : *pb_config->mutable_commands())
    _resolve_template(_pb_helper[&c], _pb_templates[object::command]);

  for (Connector& c : *pb_config->mutable_connectors())
    _resolve_template(_pb_helper[&c], _pb_templates[object::connector]);

  for (Contact& c : *pb_config->mutable_contacts())
    _resolve_template(_pb_helper[&c], _pb_templates[object::contact]);

  for (Contactgroup& cg : *pb_config->mutable_contactgroups())
    _resolve_template(_pb_helper[&cg], _pb_templates[object::contactgroup]);

  for (Host& h : *pb_config->mutable_hosts())
    _resolve_template(_pb_helper[&h], _pb_templates[object::host]);

  for (Service& s : *pb_config->mutable_services())
    _resolve_template(_pb_helper[&s], _pb_templates[object::service]);

  for (Anomalydetection& a : *pb_config->mutable_anomalydetections())
    _resolve_template(_pb_helper[&a], _pb_templates[object::anomalydetection]);

  for (Serviceescalation& se : *pb_config->mutable_serviceescalations())
    _resolve_template(_pb_helper[&se],
                      _pb_templates[object::serviceescalation]);

  for (Hostescalation& he : *pb_config->mutable_hostescalations())
    _resolve_template(_pb_helper[&he], _pb_templates[object::hostescalation]);

  try {
    for (const Command& c : pb_config->commands())
      _pb_helper.at(&c)->check_validity();

    for (const Contact& c : pb_config->contacts())
      _pb_helper.at(&c)->check_validity();

    for (const Contactgroup& cg : pb_config->contactgroups())
      _pb_helper.at(&cg)->check_validity();

    for (const Host& h : pb_config->hosts())
      _pb_helper.at(&h)->check_validity();

    for (const Hostdependency& hd : pb_config->hostdependencies())
      _pb_helper.at(&hd)->check_validity();

    for (const Hostescalation& he : pb_config->hostescalations())
      _pb_helper.at(&he)->check_validity();

    for (const Hostgroup& hg : pb_config->hostgroups())
      _pb_helper.at(&hg)->check_validity();

    for (const Service& s : pb_config->services())
      _pb_helper.at(&s)->check_validity();

    for (const Hostdependency& hd : pb_config->hostdependencies())
      _pb_helper.at(&hd)->check_validity();

    for (const Servicedependency& sd : pb_config->servicedependencies())
      _pb_helper.at(&sd)->check_validity();

    for (const Servicegroup& sg : pb_config->servicegroups())
      _pb_helper.at(&sg)->check_validity();

    for (const Timeperiod& t : pb_config->timeperiods())
      _pb_helper.at(&t)->check_validity();

    for (const Anomalydetection& a : pb_config->anomalydetections())
      _pb_helper.at(&a)->check_validity();

    for (const Tag& t : pb_config->tags())
      _pb_helper.at(&t)->check_validity();

    for (const Servicegroup& sg : pb_config->servicegroups())
      _pb_helper.at(&sg)->check_validity();

    for (const Severity& sv : pb_config->severities())
      _pb_helper.at(&sv)->check_validity();

    for (const Tag& t : pb_config->tags())
      _pb_helper.at(&t)->check_validity();

    for (const Serviceescalation& se : pb_config->serviceescalations())
      _pb_helper.at(&se)->check_validity();

    for (const Hostescalation& he : pb_config->hostescalations())
      _pb_helper.at(&he)->check_validity();

    for (const Connector& c : pb_config->connectors())
      _pb_helper.at(&c)->check_validity();

  } catch (const std::exception& e) {
    throw engine_error() << e.what();
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

  for (uint32_t i = 0; i < _lst_objects.size(); ++i) {
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

  for (uint32_t i = 0; i < _map_objects.size(); ++i) {
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
                         << " already exists";
  _map_objects[obj->type()][(real.get()->*ptr)()] = real;
}

/**
 * @brief Clean the configuration:
 *    * remove template objects.
 *
 * @param pb_config
 */
void parser::_cleanup(State* pb_config) {
  int i = 0;
  for (auto it = pb_config->mutable_services()->begin();
       it != pb_config->mutable_services()->end();) {
    if (!it->obj().register_()) {
      pb_config->mutable_services()->erase(it);
      it = pb_config->mutable_services()->begin() + i;
    } else {
      ++it;
      ++i;
    }
  }
  i = 0;
  for (auto it = pb_config->mutable_anomalydetections()->begin();
       it != pb_config->mutable_anomalydetections()->end();) {
    if (!it->obj().register_()) {
      pb_config->mutable_anomalydetections()->erase(it);
      it = pb_config->mutable_anomalydetections()->begin() + i;
    } else {
      ++it;
      ++i;
    }
  }
}
