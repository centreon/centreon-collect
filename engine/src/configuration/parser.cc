/**
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
#include "com/centreon/engine/exceptions/error.hh"
#include "com/centreon/engine/log_v2.hh"
#include "com/centreon/engine/string.hh"
#include "com/centreon/io/directory_entry.hh"
#include "configuration/anomalydetection_helper.hh"
#include "configuration/command_helper.hh"
#include "configuration/contact_helper.hh"
#include "configuration/host_helper.hh"
#include "configuration/hostgroup_helper.hh"
#include "configuration/service_helper.hh"
#include "configuration/servicedependency_helper.hh"
#include "configuration/servicegroup_helper.hh"
#include "configuration/state-generated.hh"
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

static bool fill_pair_string_group(PairStringSet* grp,
                                   const absl::string_view& value) {
  auto arr = absl::StrSplit(value, ',');

  bool first = true;
  auto itfirst = arr.begin();
  if (itfirst == arr.end())
    return true;

  do {
    auto itsecond = itfirst;
    ++itsecond;
    if (itsecond == arr.end())
      return false;
    absl::string_view v1 = absl::StripAsciiWhitespace(*itfirst);
    absl::string_view v2 = absl::StripAsciiWhitespace(*itsecond);
    if (first) {
      if (v1[0] == '+') {
        grp->set_additive(true);
        v1 = v1.substr(1);
      }
      first = false;
    }
    bool found = false;
    for (auto& m : grp->data()) {
      if (*itfirst == m.first() && *itsecond == m.second()) {
        found = true;
        break;
      }
    }
    if (!found) {
      auto* p = grp->mutable_data()->Add();
      p->set_first(v1.data(), v1.size());
      p->set_second(v2.data(), v2.size());
    }
    itfirst = itsecond;
    ++itfirst;
  } while (itfirst != arr.end());
  return true;
}

static void fill_string_group(StringSet* grp, const absl::string_view& value) {
  auto arr = absl::StrSplit(value, ',');
  bool first = true;
  for (absl::string_view d : arr) {
    d = absl::StripAsciiWhitespace(d);
    if (first) {
      if (d[0] == '+') {
        grp->set_additive(true);
        d = d.substr(1);
      }
      first = false;
    }
    bool found = false;
    for (auto& v : grp->data()) {
      if (v == d) {
        found = true;
        break;
      }
    }
    if (!found)
      grp->add_data(d.data(), d.size());
  }
}

static void fill_string_group(StringList* grp, const absl::string_view& value) {
  auto arr = absl::StrSplit(value, ',');
  bool first = true;
  for (absl::string_view d : arr) {
    d = absl::StripAsciiWhitespace(d);
    if (first) {
      if (d[0] == '+') {
        grp->set_additive(true);
        d = d.substr(1);
      }
      first = false;
    }
    grp->add_data(d.data(), d.size());
  }
}

bool set(
    State* msg,
    const absl::string_view& key,
    const absl::string_view& value,
    const absl::flat_hash_map<std::string, std::string>& correspondance = {}) {
  const Descriptor* desc = msg->GetDescriptor();
  const FieldDescriptor* f;
  const Reflection* refl;

  f = desc->FindFieldByName(std::string(key.data(), key.size()));
  if (f == nullptr) {
    auto it = correspondance.find(key);
    if (it != correspondance.end())
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
 * not exist, the correspondance table may be used to find a replacement of
 * the key. The function converts the value to the appropriate type.
 *
 * Another important point is that many configuration object contain the Object
 * obj message (something like an inheritance). This message contains three
 * fields name, use and register that are important for templating. If keys are
 * one of these names, the function tries to work directly with the obj message.
 *
 * @tparam T The type of the message containing the object key.
 * @param msg The message containing the object key.
 * @param key The key to localize the object to set.
 * @param value The value as string that will be converted to the good type.
 * @param correspondance A hash table giving traductions from keys to others.
 * If a key fails, correspondance is used to find a new replacement key.
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
    auto it = helper->correspondance().find(key);
    if (it != helper->correspondance().end())
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
  std::unique_ptr<message_helper> msg_helper;

  int current_line = 1;
  std::string type;
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
        /* cases where msg must be moved... for example when the message is
         * stored in a map. */
        if (type == "contact") {
          auto* cts = pb_config->mutable_contacts();
          Contact* ct = static_cast<Contact*>(msg);
          Contact& c = (*cts)[ct->contact_name()];
          c = std::move(*ct);
          delete msg;
          msg = &c;
          msg_helper->set_obj(msg);
        }
        const Descriptor* desc = msg->GetDescriptor();
        const FieldDescriptor* f = desc->FindFieldByName("obj");
        const Reflection* refl = msg->GetReflection();
        if (f) {
          const Object& obj =
              *static_cast<const Object*>(&refl->GetMessage(*msg, f));
          auto otype = msg_helper->otype();
          _pb_helper[msg] = std::move(msg_helper);
          if (!obj.name().empty()) {
            pb_map_object& tmpl = _pb_templates[otype];
            auto it = tmpl.find(obj.name());
            if (it != tmpl.end())
              throw engine_error() << fmt::format(
                  "Parsing of '{}' failed {}: {} already exists", type,
                  "file_info" /*_get_file_info(obj.get()) */, obj.name());
            tmpl[obj.name()] = msg;
          }
        }
        msg = nullptr;
        hook = nullptr;
      } else {
        /* Main part where keys/values are read */
        /* ------------------------------------ */
        size_t pos = l.find_first_of(" \t");
        absl::string_view key = l.substr(0, pos);
        l.remove_prefix(pos);
        l = absl::StripLeadingAsciiWhitespace(l);
        bool retval = false;
        /* particular cases with hook */
        if (hook)
          retval = hook(key, l);

        if (!retval) {
          /* Classical part */
          if (!set(msg_helper, key, l)) {
            if (key[0] != '_')
              throw engine_error() << fmt::format(
                  "Unable to parse '{}' key with value '{}' in message of type "
                  "'{}'",
                  key, l, type);
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
      if (type == "contact") {
        msg = new Contact();
        msg_helper =
            std::make_unique<contact_helper>(static_cast<Contact*>(msg));
      } else if (type == "host") {
        msg = pb_config->mutable_hosts()->Add();
        msg_helper = std::make_unique<host_helper>(static_cast<Host*>(msg));
      } else if (type == "service") {
        msg = pb_config->mutable_services()->Add();
        msg_helper =
            std::make_unique<service_helper>(static_cast<Service*>(msg));
        hook = [svc = static_cast<Service*>(msg)](
                   const absl::string_view& key,
                   const absl::string_view& value) -> bool {
          if (key == "hostgroups") {
            fill_string_group(svc->mutable_hostgroups(), value);
            return true;
          } else if (key == "contact_groups") {
            fill_string_group(svc->mutable_contactgroups(), value);
            return true;
          } else if (key == "notification_options") {
            unsigned short options(action_svc_none);
            auto values = absl::StrSplit(value, ',');
            for (auto it = values.begin(); it != values.end(); ++it) {
              absl::string_view v = absl::StripAsciiWhitespace(*it);
              if (v == "u" || v == "unknown")
                options |= action_svc_unknown;
              else if (v == "w" || v == "warning")
                options |= action_svc_warning;
              else if (v == "c" || v == "critical")
                options |= action_svc_critical;
              else if (v == "r" || v == "recovery")
                options |= action_svc_ok;
              else if (v == "f" || v == "flapping")
                options |= action_svc_flapping;
              else if (v == "s" || v == "downtime")
                options |= action_svc_downtime;
              else if (v == "n" || v == "none")
                options = action_svc_none;
              else if (v == "a" || v == "all")
                options = action_svc_unknown | action_svc_warning |
                          action_svc_critical | action_svc_ok |
                          action_svc_flapping | action_svc_downtime;
              else
                return false;
            }
            svc->set_notification_options(options);
            return true;
          }
          return false;
        };
      } else if (type == "anomalydetection") {
        msg = pb_config->mutable_anomalydetections()->Add();
        msg_helper = std::make_unique<anomalydetection_helper>(
            static_cast<Anomalydetection*>(msg));
        hook = [ad = static_cast<Anomalydetection*>(msg)](
                   const absl::string_view& key,
                   const absl::string_view& value) -> bool {
          if (key == "contact_groups") {
            fill_string_group(ad->mutable_contactgroups(), value);
            return true;
          } else if (key == "contacts") {
            fill_string_group(ad->mutable_contacts(), value);
            return true;
          } else if (key == "servicegroups") {
            fill_string_group(ad->mutable_servicegroups(), value);
            return true;
          } else if (key == "notification_options") {
            unsigned short options(action_svc_none);
            auto values = absl::StrSplit(value, ',');
            for (auto it = values.begin(); it != values.end(); ++it) {
              absl::string_view v = absl::StripAsciiWhitespace(*it);
              if (v == "u" || v == "unknown")
                options |= action_svc_unknown;
              else if (v == "w" || v == "warning")
                options |= action_svc_warning;
              else if (v == "c" || v == "critical")
                options |= action_svc_critical;
              else if (v == "r" || v == "recovery")
                options |= action_svc_ok;
              else if (v == "f" || v == "flapping")
                options |= action_svc_flapping;
              else if (v == "s" || v == "downtime")
                options |= action_svc_downtime;
              else if (v == "n" || v == "none")
                options = action_svc_none;
              else if (v == "a" || v == "all")
                options = action_svc_unknown | action_svc_warning |
                          action_svc_critical | action_svc_ok |
                          action_svc_flapping | action_svc_downtime;
              else
                return false;
            }
            ad->set_notification_options(options);
            return true;
          }
          return false;
        };
      } else if (type == "servicedependency") {
        msg = pb_config->mutable_servicedependencies()->Add();
        msg_helper = std::make_unique<servicedependency_helper>(
            static_cast<Servicedependency*>(msg));
      } else if (type == "timeperiod") {
        msg = pb_config->mutable_timeperiods()->Add();
        msg_helper =
            std::make_unique<timeperiod_helper>(static_cast<Timeperiod*>(msg));
        hook = [tp = static_cast<Timeperiod*>(msg)](
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
            if (key == "sunday")
              tp->mutable_timeranges()->mutable_sunday()->Add(std::move(tr));
            else if (key == "monday")
              tp->mutable_timeranges()->mutable_monday()->Add(std::move(tr));
            else if (key == "tuesday")
              tp->mutable_timeranges()->mutable_tuesday()->Add(std::move(tr));
            else if (key == "wednesday")
              tp->mutable_timeranges()->mutable_wednesday()->Add(std::move(tr));
            else if (key == "thursday")
              tp->mutable_timeranges()->mutable_thursday()->Add(std::move(tr));
            else if (key == "friday")
              tp->mutable_timeranges()->mutable_friday()->Add(std::move(tr));
            else if (key == "saturday")
              tp->mutable_timeranges()->mutable_saturday()->Add(std::move(tr));
            else
              return false;
          }
          return true;
        };
      } else if (type == "command") {
        msg = pb_config->mutable_commands()->Add();
        msg_helper =
            std::make_unique<command_helper>(static_cast<Command*>(msg));
      } else if (type == "hostgroup") {
        msg = pb_config->mutable_hostgroups()->Add();
        msg_helper =
            std::make_unique<hostgroup_helper>(static_cast<Hostgroup*>(msg));
      } else if (type == "servicegroup") {
        msg = pb_config->mutable_servicegroups()->Add();
        msg_helper = std::make_unique<servicegroup_helper>(
            static_cast<Servicegroup*>(msg));
        hook = [sg = static_cast<Servicegroup*>(msg)](
                   const absl::string_view& key,
                   const absl::string_view& value) -> bool {
          if (key == "members")
            return fill_pair_string_group(sg->mutable_members(), value);
          else
            return false;
        };
      } else if (type == "tag") {
        msg = pb_config->mutable_tags()->Add();
        msg_helper = std::make_unique<tag_helper>(static_cast<Tag*>(msg));
        hook = [tg = static_cast<Tag*>(msg)](
                   const absl::string_view& key,
                   const absl::string_view& value) -> bool {
          bool retval = true;
          if (key == "id" || key == "tag_id") {
            uint64_t id;
            if (absl::SimpleAtoi(value, &id))
              tg->mutable_key()->set_id(id);
            else
              retval = false;
          } else if (key == "type" || key == "tag_type") {
            if (value == "hostcategory")
              tg->mutable_key()->set_type(tag::hostcategory);
            else if (value == "servicecategory")
              tg->mutable_key()->set_type(tag::servicecategory);
            else if (value == "hostgroup")
              tg->mutable_key()->set_type(tag::hostgroup);
            else if (value == "servicegroup")
              tg->mutable_key()->set_type(tag::servicegroup);
            else
              retval = false;
          } else
            retval = false;
          return retval;
        };
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
                log_v2::config()->error(
                    "New content of StringSet {}: {}", f->name(),
                    fmt::join(set->data().begin(), set->data().end(), ","));

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
                log_v2::config()->error("New content of StringList {}: {}",
                                        f->name(), fmt::join(lst->data(), ","));
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
    _resolve_template(_pb_helper[it->second], tmpls);
    _merge(msg_helper, it->second);
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
 * @brief Search for each string given in mandatory array and check that
 * the field with that string as key is not empty.
 * In case one field is empty, this function throws an exception.
 *
 * @param msg The Protobuf message to check.
 * @param mandatory An array of strings terminated with nullptr.
 */
void parser::_check_validity(const Message& msg,
                             const char* const* mandatory) const {
  const Descriptor* desc = msg.GetDescriptor();
  const Reflection* refl = msg.GetReflection();
  std::string tmpl;

  const FieldDescriptor* f;
  //  const FieldDescriptor* f = desc->FindFieldByName("obj");
  //  if (f) {
  //    const Object& obj = static_cast<const Object&>(refl->GetMessage(msg,
  //    f)); if (!obj.register_())
  //      return;
  //  }
  for (auto field = mandatory; *field; ++field) {
    f = desc->FindFieldByName(*field);
    if (f) {
      if (!f->is_repeated()) {
        switch (f->cpp_type()) {
          case FieldDescriptor::CPPTYPE_STRING: {
            if (refl->GetStringReference(msg, f, &tmpl).empty())
              throw engine_error() << fmt::format(
                  "{} has its property '{}' empty which is mandatory",
                  desc->name(), f->name());
          } break;
          default:
            log_v2::config()->error(
                "Type '{}' not implemented in check_validity", f->type_name());
            assert(192 == 1897);
        }
      } else {
        log_v2::config()->error(
            "Repeated type '{}' not implemented in check_validity",
            f->type_name());
        assert(18972 == 9);
      }
    }
  }
}

void parser::_resolve_template(State* pb_config) {
  for (Command& c : *pb_config->mutable_commands())
    _resolve_template(_pb_helper[&c], _pb_templates[object::command]);

  for (Connector& c : *pb_config->mutable_connectors())
    _resolve_template(_pb_helper[&c], _pb_templates[object::connector]);

  auto* contacts = pb_config->mutable_contacts();
  for (auto it = contacts->begin(); it != contacts->end(); ++it)
    _resolve_template(_pb_helper[&it->second], _pb_templates[object::contact]);

  for (Contactgroup& cg : *pb_config->mutable_contactgroups())
    _resolve_template(_pb_helper[&cg], _pb_templates[object::contactgroup]);

  for (Service& s : *pb_config->mutable_services())
    _resolve_template(_pb_helper[&s], _pb_templates[object::service]);

  for (Anomalydetection& a : *pb_config->mutable_anomalydetections())
    _resolve_template(_pb_helper[&a], _pb_templates[object::anomalydetection]);

  {
    constexpr std::array<const char*, 3> mandatory{"command_name",
                                                   "command_line", nullptr};
    for (const Command& c : pb_config->commands())
      _check_validity(c, mandatory.data());
  }
  {
    constexpr std::array<const char*, 2> mandatory{"contact_name", nullptr};
    for (auto it = pb_config->contacts().begin();
         it != pb_config->contacts().end(); ++it) {
      _check_validity(it->second, mandatory.data());
    }
  }
  {
    constexpr std::array<const char*, 2> mandatory{"contactgroup_name",
                                                   nullptr};
    for (const Contactgroup& c : pb_config->contactgroups())
      _check_validity(c, mandatory.data());
  }
  {
    constexpr std::array<const char*, 3> mandatory{"host_name", "address",
                                                   nullptr};
    for (const Host& h : pb_config->hosts())
      _check_validity(h, mandatory.data());
  }
  {
    constexpr std::array<const char*, 3> mandatory{"host_name", "address",
                                                   nullptr};
    for (const Hostdependency& hd : pb_config->hostdependencies())
      _check_validity(hd, mandatory.data());
  }
  {
    for (const Hostescalation& he : pb_config->hostescalations()) {
      if (_is_registered(he)) {
        if (he.hosts().data().empty() && he.hostgroups().data().empty())
          throw engine_error()
              << "Host escalation must contain at least one of the fields "
                 "'hosts' or 'hostgroups' not empty";
      }
    }
  }
  {
    constexpr std::array<const char*, 2> mandatory{"hostgroup_name", nullptr};
    for (const Hostgroup& hg : pb_config->hostgroups())
      _check_validity(hg, mandatory.data());
  }
  {
    for (const Service& s : pb_config->services()) {
      if (_is_registered(s)) {
        if (s.service_description().empty())
          throw engine_error() << "Services must have a non-empty description";
        if (s.check_command().empty())
          throw engine_error()
              << fmt::format("Service '{}' has an empty check command",
                             s.service_description());
        if (s.hosts().data().empty() && s.hostgroups().data().empty())
          throw engine_error() << fmt::format(
              "Service '{}' must contain at least one of the fields 'hosts' or "
              "'hostgroups' not empty",
              s.service_description());
      }
    }
  }
  {
    for (const Servicedependency& sd : pb_config->servicedependencies())
      if (sd.servicegroups().data().empty() &&
          (sd.service_description().data().empty() ||
           (sd.hosts().data().empty() && sd.hostgroups().data().empty())))
        throw engine_error()
            << "Service escalation is not attached to any service or service "
               "group or host or host group";
  }
  {
    constexpr std::array<const char*, 2> mandatory{"servicegroup_name",
                                                   nullptr};
    for (const Servicegroup& sg : pb_config->servicegroups())
      _check_validity(sg, mandatory.data());
  }
  {
    constexpr std::array<const char*, 2> mandatory{"timeperiod_name", nullptr};
    for (const Timeperiod& t : pb_config->timeperiods())
      _check_validity(t, mandatory.data());
  }
  {
    constexpr std::array<const char*, 5> mandatory{"service_description",
                                                   "host_name", "metric_name",
                                                   "thresholds_file", nullptr};
    for (const Anomalydetection& a : pb_config->anomalydetections())
      if (_is_registered(a))
        _check_validity(a, mandatory.data());
  }
  {
    for (const Tag& t : pb_config->tags()) {
      if (t.tag_name().empty())
        throw engine_error() << "Tag cannot have a tag name empty";
      if (t.key().id() == 0)
        throw engine_error()
            << fmt::format("Tag '{}' has a null id", t.tag_name());
    }
  }
  {
    constexpr std::array<const char*, 2> mandatory{"servicegroup_name",
                                                   nullptr};
    for (const Servicegroup& sg : pb_config->servicegroups())
      _check_validity(sg, mandatory.data());
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
