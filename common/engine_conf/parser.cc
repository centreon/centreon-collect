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
#include <absl/strings/match.h>
#include <filesystem>
#include "anomalydetection_helper.hh"
#include "com/centreon/common/file.hh"
#include "com/centreon/exceptions/msg_fmt.hh"
#include "command_helper.hh"
#include "common/engine_conf/state.pb.h"
#include "common/log_v2/log_v2.hh"
#include "connector_helper.hh"
#include "contact_helper.hh"
#include "contactgroup_helper.hh"
#include "host_helper.hh"
#include "hostdependency_helper.hh"
#include "hostescalation_helper.hh"
#include "hostgroup_helper.hh"
#include "message_helper.hh"
#include "service_helper.hh"
#include "servicedependency_helper.hh"
#include "serviceescalation_helper.hh"
#include "servicegroup_helper.hh"
#include "severity_helper.hh"
#include "state_helper.hh"
#include "tag_helper.hh"
#include "timeperiod_helper.hh"

using namespace com::centreon;
using namespace com::centreon::engine::configuration;

using com::centreon::common::log_v2::log_v2;
using com::centreon::exceptions::msg_fmt;
using ::google::protobuf::Descriptor;
using ::google::protobuf::FieldDescriptor;
using ::google::protobuf::Message;
using ::google::protobuf::Reflection;

/**
 *  Default constructor.
 *
 */
parser::parser() : _logger{log_v2::instance().get(log_v2::CONFIG)} {}

/**
 *  Parse configuration file.
 *
 *  @param[in] path   The configuration file path.
 *  @param[in] pb_config The state configuration to fill.
 *  @param[out] err   The config warnings/errors counter.
 */
void parser::parse(const std::string& path, State* pb_config, error_cnt& err) {
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
  _resolve_template(pb_config, err);

  _cleanup(pb_config);
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

/**
 *  Parse the directory configuration.
 *
 *  @param[in] path The directory path.
 */
void parser::_parse_directory_configuration(const std::string& path,
                                            State* pb_config) {
  for (auto& entry : std::filesystem::directory_iterator(path)) {
    if (entry.is_regular_file() && entry.path().extension() == ".cfg")
      _parse_object_definitions(entry.path().string(), pb_config);
  }
}

/**
 *  Parse the global configuration file.
 *
 *  @param[in] path The configuration path.
 */
void parser::_parse_global_configuration(const std::string& path,
                                         State* pb_config) {
  _logger->info("Reading main configuration file '{}'.", path);

  std::filesystem::path p(path);
  if (!std::filesystem::is_regular_file(p)) {
    _logger->info("No configuration file available at '{}'.", path);
    return;
  }
  std::string content = common::read_file_content(path);

  pb_config->set_cfg_main(path);
  _current_line = 0;
  _current_path = path;

  auto tab{absl::StrSplit(content, '\n')};
  state_helper* cfg_helper =
      static_cast<state_helper*>(_pb_helper[pb_config].get());
  for (auto it = tab.begin(); it != tab.end(); ++it) {
    std::string_view l = absl::StripAsciiWhitespace(*it);
    if (l.empty() || l[0] == '#')
      continue;
    std::pair<std::string_view, std::string_view> p =
        absl::StrSplit(l, absl::MaxSplits('=', 1));
    p.first = absl::StripTrailingAsciiWhitespace(p.first);
    p.second = absl::StripLeadingAsciiWhitespace(p.second);
    bool retval = false;
    /* particular cases with hook */
    retval = cfg_helper->hook(p.first, p.second);
    if (!retval) {
      if (!cfg_helper->set_global(p.first, p.second))
        _logger->error("Unable to parse '{}' key with value '{}'", p.first,
                       p.second);
    }
  }

  /* A bad hook so that Engine stays compatible with previous
   * versions. */
  for (auto& m : pb_config->broker_module()) {
    if (absl::StrContains(m, "cbmod.so")) {
      auto arr = absl::StrSplit(m, absl::ByAnyChar(" \t\n"), absl::SkipEmpty());
      auto it = arr.begin();
      std::string_view module_name;
      if (it != arr.end()) {
        module_name = *it;
        ++it;
        if (it != arr.end()) {
          std::string_view module_args = *it;
          module_args = absl::StripAsciiWhitespace(module_args);
          if (pb_config->broker_module_cfg_file().empty()) {
            pb_config->set_broker_module_cfg_file(
                std::string(module_args.data(), module_args.size()));
            _logger->warn(
                "Parsing the configuration file '{}' of the 'cbmod' module "
                "to "
                "still be able to use it.",
                module_args);
          } else {
            _logger->warn(
                "The new 'broker_module_cfg_file' field is already set, so "
                "nothing done with the 'cbmod' module configuration file '{}'.",
                module_args);
          }
        }
      }
      break;
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
 * * A hastable <string, string> named correspondence is used in case of
 * several keys to access to the same value. This is, for example, the case
 * for host_id which is historically also named _HOST_ID.
 * * A std::function<bool(string_view_string_view) can also be defined in
 *   several cases to make special stuffs. For example, we use it for
 * timeperiod object to set its timeranges.
 *
 * @param path The file to parse.
 * @param pb_config The configuration to complete.
 */
void parser::_parse_object_definitions(const std::string& path,
                                       State* pb_config) {
  _logger->info("Processing object config file '{}'", path);

  std::string content = common::read_file_content(path);

  auto tab{absl::StrSplit(content, '\n')};
  std::string ll;
  bool append_to_previous_line = false;
  std::unique_ptr<Message> msg;
  std::unique_ptr<message_helper> msg_helper;

  int current_line = 1;
  std::string type;

  for (auto it = tab.begin(); it != tab.end(); ++it, current_line++) {
    std::string_view l = absl::StripAsciiWhitespace(*it);
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
              throw msg_fmt(
                  "Parsing of '{}' failed in cfg file: {} already exists", type,
                  obj.name());
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
                _logger->critical("Attempt to add an object of unknown type");
            }
          }
        }
        msg = nullptr;
      } else {
        /* Main part where keys/values are read */
        /* ------------------------------------ */
        size_t pos = l.find_first_of(" \t");
        std::string_view key = l.substr(0, pos);
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
          if (!msg_helper->set(key, l)) {
            if (!msg_helper->insert_customvariable(key, l))
              throw msg_fmt(
                  "Unable to parse '{}' key with value '{}' in message of "
                  "type "
                  "'{}'",
                  key, l, type);
          }
        }
      }
    } else {
      if (!absl::StartsWith(l, "define") || !std::isspace(l[6]))
        throw msg_fmt(
            "Parsing of object definition failed in file '{}' at line {}: "
            "Unexpected start definition",
            path, current_line);
      /* Let's remove the first 6 characters ("define") */
      l = absl::StripLeadingAsciiWhitespace(l.substr(6));
      if (l.empty() || l[l.size() - 1] != '{')
        throw msg_fmt(
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
        _logger->error("Type '{}' not yet supported by the parser", type);
        assert(1 == 18);
      }
    }
  }
}

/**
 *  Parse the resource file.
 *
 *  @param[in] path The resource file path.
 */
void parser::_parse_resource_file(const std::string& path, State* pb_config) {
  _logger->info("Reading resource file '{}'", path);

  std::string content = common::read_file_content(path);

  auto tab{absl::StrSplit(content, '\n')};
  int current_line = 1;
  for (auto it = tab.begin(); it != tab.end(); ++it, current_line++) {
    std::string_view l = absl::StripLeadingAsciiWhitespace(*it);
    if (l.empty() || l[0] == '#' || l[0] == ';')
      continue;
    std::pair<std::string_view, std::string_view> p =
        absl::StrSplit(l, absl::MaxSplits('=', 1));
    p.first = absl::StripTrailingAsciiWhitespace(p.first);
    p.second = absl::StripLeadingAsciiWhitespace(p.second);
    if (p.first.size() >= 3 && p.first[0] == '$' &&
        p.first[p.first.size() - 1] == '$') {
      p.first = p.first.substr(1, p.first.size() - 2);
      (*pb_config
            ->mutable_users())[std::string(p.first.data(), p.first.size())] =
          std::string(p.second.data(), p.second.size());
    } else
      throw msg_fmt("Invalid user key '{}'", p.first);
  }
}

/**
 * @brief For each type of object in the State, templates are resolved that is
 * to say, children inherite from parents properties.
 *
 * @param pb_config The State containing all the object to handle.
 * @param err The config warnings/errors counter.
 */
void parser::_resolve_template(State* pb_config, error_cnt& err) {
  for (Command& c : *pb_config->mutable_commands())
    _resolve_template(_pb_helper[&c], _pb_templates[message_helper::command]);

  for (Connector& c : *pb_config->mutable_connectors())
    _resolve_template(_pb_helper[&c], _pb_templates[message_helper::connector]);

  for (Contact& c : *pb_config->mutable_contacts())
    _resolve_template(_pb_helper[&c], _pb_templates[message_helper::contact]);

  for (Contactgroup& cg : *pb_config->mutable_contactgroups())
    _resolve_template(_pb_helper[&cg],
                      _pb_templates[message_helper::contactgroup]);

  for (Host& h : *pb_config->mutable_hosts())
    _resolve_template(_pb_helper[&h], _pb_templates[message_helper::host]);

  for (Service& s : *pb_config->mutable_services())
    _resolve_template(_pb_helper[&s], _pb_templates[message_helper::service]);

  for (Anomalydetection& a : *pb_config->mutable_anomalydetections())
    _resolve_template(_pb_helper[&a],
                      _pb_templates[message_helper::anomalydetection]);

  for (Serviceescalation& se : *pb_config->mutable_serviceescalations())
    _resolve_template(_pb_helper[&se],
                      _pb_templates[message_helper::serviceescalation]);

  for (Hostescalation& he : *pb_config->mutable_hostescalations())
    _resolve_template(_pb_helper[&he],
                      _pb_templates[message_helper::hostescalation]);

  for (Hostgroup& hg : *pb_config->mutable_hostgroups())
    _resolve_template(_pb_helper[&hg],
                      _pb_templates[message_helper::hostgroup]);

  for (Servicegroup& sg : *pb_config->mutable_servicegroups())
    _resolve_template(_pb_helper[&sg],
                      _pb_templates[message_helper::servicegroup]);

  for (const Command& c : pb_config->commands())
    _pb_helper.at(&c)->check_validity(err);

  for (const Contact& c : pb_config->contacts())
    _pb_helper.at(&c)->check_validity(err);

  for (const Contactgroup& cg : pb_config->contactgroups())
    _pb_helper.at(&cg)->check_validity(err);

  for (const Host& h : pb_config->hosts())
    _pb_helper.at(&h)->check_validity(err);

  for (const Hostdependency& hd : pb_config->hostdependencies())
    _pb_helper.at(&hd)->check_validity(err);

  for (const Hostescalation& he : pb_config->hostescalations())
    _pb_helper.at(&he)->check_validity(err);

  for (const Hostgroup& hg : pb_config->hostgroups())
    _pb_helper.at(&hg)->check_validity(err);

  for (const Service& s : pb_config->services())
    _pb_helper.at(&s)->check_validity(err);

  for (const Hostdependency& hd : pb_config->hostdependencies())
    _pb_helper.at(&hd)->check_validity(err);

  for (const Servicedependency& sd : pb_config->servicedependencies())
    _pb_helper.at(&sd)->check_validity(err);

  for (const Servicegroup& sg : pb_config->servicegroups())
    _pb_helper.at(&sg)->check_validity(err);

  for (const Timeperiod& t : pb_config->timeperiods())
    _pb_helper.at(&t)->check_validity(err);

  for (const Anomalydetection& a : pb_config->anomalydetections())
    _pb_helper.at(&a)->check_validity(err);

  for (const Tag& t : pb_config->tags())
    _pb_helper.at(&t)->check_validity(err);

  for (const Servicegroup& sg : pb_config->servicegroups())
    _pb_helper.at(&sg)->check_validity(err);

  for (const Severity& sv : pb_config->severities())
    _pb_helper.at(&sv)->check_validity(err);

  for (const Tag& t : pb_config->tags())
    _pb_helper.at(&t)->check_validity(err);

  for (const Serviceescalation& se : pb_config->serviceescalations())
    _pb_helper.at(&se)->check_validity(err);

  for (const Hostescalation& he : pb_config->hostescalations())
    _pb_helper.at(&he)->check_validity(err);

  for (const Connector& c : pb_config->connectors())
    _pb_helper.at(&c)->check_validity(err);
}

/**
 * @brief Resolvers a message given by its helper and using the given
 * templates.
 *
 * @param msg_helper The message helper.
 * @param tmpls The templates to use.
 */
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
      throw msg_fmt("Cannot merge object of type '{}'", u);
    _resolve_template(_pb_helper[it->second.get()], tmpls);
    _merge(msg_helper, it->second.get());
  }
}

/**
 * @brief For each unchanged field in the Protobuf object stored in
 * msg_helper, we copy the corresponding field from tmpl. This is the key for
 * the inheritence with cfg files.
 *
 * @param msg_helper A message_help holding a protobuf message
 * @param tmpl A template of the same type as the on in the msg_helper
 */
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
          !msg_helper->changed(f->index())) {
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
              msg_helper->set_changed(f->index());
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
                  if (d && d1) {
                    if (d->name() == "PairUint64_32" &&
                        d1->name() == "PairUint64_32") {
                      const PairUint64_32& p =
                          static_cast<const PairUint64_32&>(m);
                      const PairUint64_32& p1 =
                          static_cast<const PairUint64_32&>(m1);
                      if (p.first() == p1.first() &&
                          p.second() == p1.second()) {
                        found = true;
                        break;
                      }
                    } else if (d->name() == "CustomVariable" &&
                               d1->name() == "CustomVariable") {
                      const CustomVariable& cv =
                          static_cast<const CustomVariable&>(m);
                      const CustomVariable& cv1 =
                          static_cast<const CustomVariable&>(m1);
                      if (cv.name() == cv1.name()) {
                        _logger->info("same name");
                        found = true;
                        break;
                      }
                    } else {
                      assert("not good at all" == nullptr);
                    }
                  }
                }
                if (!found) {
                  Message* new_m = refl->AddMessage(msg, f);
                  new_m->CopyFrom(m);
                }
              }
              msg_helper->set_changed(f->index());
            } break;
            default:
              _logger->error(
                  "Repeated type f->cpp_type = {} not managed in the "
                  "inheritence.",
                  static_cast<uint32_t>(f->cpp_type()));
              assert(124 == 294);
          }
        } else {
          switch (f->cpp_type()) {
            case FieldDescriptor::CPPTYPE_STRING:
              refl->SetString(msg, f, refl->GetString(*tmpl, f));
              msg_helper->set_changed(f->index());
              break;
            case FieldDescriptor::CPPTYPE_BOOL:
              refl->SetBool(msg, f, refl->GetBool(*tmpl, f));
              msg_helper->set_changed(f->index());
              break;
            case FieldDescriptor::CPPTYPE_INT32:
              refl->SetInt32(msg, f, refl->GetInt32(*tmpl, f));
              msg_helper->set_changed(f->index());
              break;
            case FieldDescriptor::CPPTYPE_UINT32:
              refl->SetUInt32(msg, f, refl->GetUInt32(*tmpl, f));
              msg_helper->set_changed(f->index());
              break;
            case FieldDescriptor::CPPTYPE_UINT64:
              refl->SetUInt64(msg, f, refl->GetUInt64(*tmpl, f));
              msg_helper->set_changed(f->index());
              break;
            case FieldDescriptor::CPPTYPE_ENUM:
              refl->SetEnum(msg, f, refl->GetEnum(*tmpl, f));
              msg_helper->set_changed(f->index());
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
              } else if (d && d->name() == "PairStringSet") {
                PairStringSet* orig_pair =
                    static_cast<PairStringSet*>(refl->MutableMessage(tmpl, f));
                PairStringSet* pair =
                    static_cast<PairStringSet*>(refl->MutableMessage(msg, f));
                if (pair->additive()) {
                  for (auto& v : orig_pair->data()) {
                    bool found = false;
                    for (auto& s : *pair->mutable_data()) {
                      if (s.first() == v.first() && s.second() == v.second()) {
                        found = true;
                        break;
                      }
                    }
                    if (!found)
                      pair->add_data()->CopyFrom(v);
                  }
                } else if (pair->data().empty())
                  *pair->mutable_data() = orig_pair->data();
              } else {
                refl->MutableMessage(msg, f)->CopyFrom(
                    refl->GetMessage(*tmpl, f));
              }
              msg_helper->set_changed(f->index());
            } break;

            default:
              _logger->error("Entry '{}' of type {} not managed in merge",
                             f->name(), f->type_name());
              assert(123 == 293);
          }
        }
      }
    }
  }
}

/**
 * @brief Adapt the Engine configuration given in centengine_cfg file to the
 * directory containing it and write it in centengine_test. It is mandatory to
 * parse it from this directory.
 *
 * @param centengine_test The test configuration file to build.
 * @param centengine_cfg The configuration file to adapt.
 * @param ec The error code if any.
 */
void parser::build_test_file(const std::filesystem::path& centengine_test,
                             const std::filesystem::path& centengine_cfg,
                             std::error_code& ec) {
  std::string content;
  try {
    content = common::read_file_content(centengine_cfg);
    auto directory = centengine_test.parent_path();
    std::ofstream f(centengine_test);
    auto tab{absl::StrSplit(content, '\n', absl::SkipEmpty())};
    for (auto& line : tab) {
      if (absl::StartsWith(line, "cfg_file=")) {
        std::filesystem::path p(line.substr(8));
        f << "cfg_file=" << (directory / p.filename()).string() << std::endl;
      } else if (absl::StartsWith(line, "resource_file=")) {
        std::filesystem::path p(line.substr(13));
        f << "resource_file=" << (directory / p.filename()).string()
          << std::endl;
      } else {
        f << line << std::endl;
      }
    }
  } catch (std::exception& e) {
    ec = std::make_error_code(std::errc::io_error);
  }
}
