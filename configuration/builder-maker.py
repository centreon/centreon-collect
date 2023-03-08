#!/usr/bin/env python3.9
#
# Copyright 2022-2023 Centreon (https://www.centreon.com/)
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# For more information : contact@centreon.com
#
import re
import os
import hashlib
import subprocess

if not os.path.exists("/tmp/configuration"):
    os.mkdir("/tmp/configuration")
class_files_hh = [
    "inc/com/centreon/engine/configuration/anomalydetection.hh",
    "inc/com/centreon/engine/configuration/command.hh",
    "inc/com/centreon/engine/configuration/connector.hh",
    "inc/com/centreon/engine/configuration/contact.hh",
    "inc/com/centreon/engine/configuration/contactgroup.hh",
    "inc/com/centreon/engine/configuration/host.hh",
    "inc/com/centreon/engine/configuration/hostdependency.hh",
    "inc/com/centreon/engine/configuration/hostescalation.hh",
    "inc/com/centreon/engine/configuration/hostgroup.hh",
    "inc/com/centreon/engine/configuration/service.hh",
    "inc/com/centreon/engine/configuration/servicedependency.hh",
    "inc/com/centreon/engine/configuration/serviceescalation.hh",
    "inc/com/centreon/engine/configuration/servicegroup.hh",
    "inc/com/centreon/engine/configuration/severity.hh",
    "inc/com/centreon/engine/configuration/tag.hh",
    "inc/com/centreon/engine/configuration/timeperiod.hh",
]

class_files_cc = [
    "src/configuration/anomalydetection.cc",
    "src/configuration/command.cc",
    "src/configuration/connector.cc",
    "src/configuration/contact.cc",
    "src/configuration/contactgroup.cc",
    "src/configuration/host.cc",
    "src/configuration/hostdependency.cc",
    "src/configuration/hostescalation.cc",
    "src/configuration/hostgroup.cc",
    "src/configuration/service.cc",
    "src/configuration/servicedependency.cc",
    "src/configuration/serviceescalation.cc",
    "src/configuration/servicegroup.cc",
    "src/configuration/severity.cc",
    "src/configuration/tag.cc",
    "src/configuration/timeperiod.cc",
]


def sync_files():
    print("### SYNC FILES")
    files = os.listdir(".")
    r = re.compile(".*_helper.(cc|hh)$")
    h = {}
    for f in files:
        if r.match(f):
            content = open(f"./{f}", 'r').read()
            hh = hashlib.md5(content.encode('utf-8'))
            h[f] = hh.hexdigest()

    files_tmp = os.listdir("/tmp/configuration")
    for f in files_tmp:
        if r.match(f):
            try:
                content = open(f"/tmp/configuration/{f}", 'r').read()
                hh = hashlib.md5(content.encode('utf-8')).hexdigest()
                if not f in h or hh != h[f]:
                    print(f"{f} changed since last modification")
                    ff = open(f"./{f}", "w")
                    ff.write(content)
                    ff.close()
            except:
                print(f"Cannot read file /tmp/configuration/{f}")


def indent_files():
    os.system(f"clang-format -i -style=chromium /tmp/configuration/*")


def complete_filehelper_cc(fhcc, cname, name, number: int, correspondence, hook, check_validity, msg_list):
    name_cap = name.capitalize()
    cname = name + "_helper"
    fhcc.write(f"""
/**
 * @brief Constructor from a {name_cap} object.
 *
 * @param obj The {name_cap} object on which this helper works. The helper is not the owner of this object.
 */
{cname}::{cname}({name_cap}* obj) : message_helper(object_type::{name}, obj, {correspondence}, {number}) {{
  _init();
}}

/**
  * @brief For several keys, the parser of {name_cap} objects has a particular
  *        behavior. These behaviors are handled here.
  * @param key The key to parse.
  * @param value The value corresponding to the key
  */
bool {cname}::hook(const absl::string_view& key, const absl::string_view& value) {{
  {cap_name}* obj = static_cast<{cap_name}*>(mut_obj());
""")
    for h in hook:
        fhcc.write(h)
    fhcc.write("""  return false;
}
""")
    fhcc.write(f"""
/**
 * @brief Check the validity of the {cap_name} object.
 */
void {cname}::check_validity() const {{
  const {cap_name}* o = static_cast<const {cap_name}*>(obj());
""")
    for c in check_validity:
        fhcc.write(c)
    fhcc.write("\n}")

    header = False
    for m in msg_list:
        if 'default' in m:
            if not header:
                cc_lines = [f"""

/**
 * @brief Initializer of the {name_cap} object, in other words set its default values.
 */
void {cname}::_init() {{
  {cap_name}* obj = static_cast<{cap_name}*>(mut_obj());
"""]
                header = True
            if m['proto_type'] == 'KeyType':
                values = m['default'].split(',')
                cc_lines.append(
                    f"  obj->mutable_{m['proto_name']}()->set_id({values[0].strip()});\n")
                cc_lines.append(
                    f"  obj->mutable_{m['proto_name']}()->set_type({values[1].strip()});\n")
            elif m['typ'] == 'point_2d':
                values = m['default'].split(',')
                cc_lines.append(
                    f"  obj->mutable_{m['proto_name']}()->set_x({values[0].strip()});\n")
                cc_lines.append(
                    f"  obj->mutable_{m['proto_name']}()->set_y({values[1].strip()});\n")
            elif m['typ'] == 'point_3d':
                values = m['default'].split(',')
                cc_lines.append(
                    f"  obj->mutable_{m['proto_name']}()->set_x({values[0].strip()});\n")
                cc_lines.append(
                    f"  obj->mutable_{m['proto_name']}()->set_y({values[1].strip()});\n")
                cc_lines.append(
                    f"  obj->mutable_{m['proto_name']}()->set_y({values[2].strip()});\n")
            else:
                cc_lines.append(
                    f"  obj->set_{m['proto_name']}({m['default']});\n")

    if not header:
        cc_lines = [f"""
void {cname}::_init() {{\n"""]
        header = True

    cc_lines.append("}\n")
    if cap_name in ["Contact", "Service", "Host", "Anomalydetection"]:
        cc_lines.append(f"""
/**
 * @brief If the provided key/value have their parsing to fail previously,
 * it is possible they are a customvariable. A customvariable name has its
 * name starting with an underscore. This method checks the possibility to
 * store a customvariable in the given object and stores it if possible.
 *
 * @param key   The name of the customvariable.
 * @param value Its value as a string.
 *
 * @return True if the customvariable has been well stored.
 */
bool {cname}::insert_customvariable(absl::string_view key, absl::string_view value) {{
  if (key[0] != '_')
    return false;
    
  key.remove_prefix(1);
  {cap_name}* obj = static_cast<{cap_name}*>(mut_obj());
  auto* cvs = obj->mutable_customvariables();
  for (auto& c : *cvs) {{
    if (c.name() == key) {{
      c.set_value(value.data(), value.size());
      return true;
    }}
  }}
  auto new_cv = cvs->Add();
  new_cv->set_name(key.data(), key.size());
  new_cv->set_value(value.data(), value.size());
  return true;
}}
""")

    cc_lines.append("}\n")
    cc_lines.append("}\n")
    cc_lines.append("}\n")
    fhcc.writelines(cc_lines)
    fhcc.write("\n}")


def prepare_filehelper_cc(name: str):
    filename = f"/tmp/configuration/{name}_helper.cc"
    fhcc = open(filename, "w")
    fhcc.write(f"""/*
 * Copyright 2022 Centreon (https://www.centreon.com/)
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
#include "configuration/{name}_helper.hh"
#include "com/centreon/exceptions/msg_fmt.hh"

using msg_fmt = com::centreon::exceptions::msg_fmt;

namespace com {{
namespace centreon {{
namespace engine {{
namespace configuration {{
""")
    return fhcc


def prepare_filehelper_hh(cap_name: str, name: str):
    fhhh = open(f"/tmp/configuration/{name}_helper.hh", "w")
    macro = name.upper().replace(".", "_")
    cname = name.replace(".hh", "")
    objname = cname.capitalize()
    fhhh.write(f"""/*
 * Copyright 2022-2023 Centreon (https://www.centreon.com/)
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

#ifndef CCE_CONFIGURATION_{macro}
#define CCE_CONFIGURATION_{macro}

#include "configuration/state-generated.pb.h"
#include "configuration/message_helper.hh"

namespace com {{
namespace centreon {{
namespace engine {{
namespace configuration {{

class {cname}_helper : public message_helper {{

  void _init();
 public:
  {cname}_helper({objname}* obj);
  ~{cname}_helper() noexcept = default;
  void check_validity() const override;

  bool hook(const absl::string_view& key, const absl::string_view& value) override;
""")
    if cap_name in ["Contact",  "Host", "Service", "Anomalydetection"]:
        fhhh.write(f"""
  bool insert_customvariable(absl::string_view key, absl::string_view value) override;
""")
    fhhh.write(f"""}};
}}  // namespace configuration
}}  // namespace engine
}}  // namespace centreon
}}  // namespace com

#endif /* !CCE_CONFIGURATION_{macro} */
""")
    return fhhh


def proto_type(t: str):
    typ = {
        "int32_t": "int32",
        "uint32_t": "uint32",
        "int64_t": "int64",
        "uint64_t": "uint64",
        "std::string": "string",
        "int": "int32",
        "bool": "bool",
        "unsigned int": "uint32",
        "unsigned short": "uint32",
        "double": "double",
        "map_customvar": "repeated CustomVariable",
        "group<set_string>": "StringSet",
        "group<list_string>": "StringList",
        "point_2d": "Point2d",
        "point_3d": "Point3d",
        "dependency_kind": "DependencyKind",
        "key_type": "KeyType",
        "days_array": "DaysArray",
        "tab_string": "repeated string",
        "exception_array": "ExceptionArray",
        "group<set_pair_string>": "PairStringSet",
        "std::set<std::pair<uint64_t, uint16_t>>": "repeated PairUint64_32",
    }
    if t in typ:
        return typ[t]
    else:
        print(f"Error: key '{t}' not recognized")
        return t


def get_messages_of_conf(header, msg: [str]):
    r = re.compile(r"^\s*([a-z0-9][a-z0-9\s_:<,>]*[a-z0-9>])\s+(_[a-z0-9_]*);")
    ropt = re.compile(r"opt<(.*)>")
    for l in header:
        m = r.match(l)
        if m and m.group(2) != '_setters':
            msg = {
                'line': l,
                "typ": m.group(1),
                "name": m.group(2),
                "optional": False,
            }
            mopt = ropt.match(m.group(1))
            if mopt:
                msg['optional'] = True
                msg['typ'] = mopt.group(1)
            msg['proto_type'] = proto_type(msg['typ'])
            msg['proto_name'] = proto_name(msg['name'])
            msg_list.append(msg)


def get_default_values(cap_name, cpp, msg: [str]):
    rmin = re.compile(
        r"(?:static)?\s*([a-z0-9][a-z_0-9\s]+[a-z0-9])\s*(?:const)?\s*default_")
    r = re.compile(
        r"(?:static)?\s*([a-z0-9][a-z_0-9\s]+[a-z0-9])\s*(?:const)?\s*(default_[a-z0-9_]*)\((.*)\);")
    rdecl = re.compile(r"^\s*(_[a-z][a-z0-9_]*)\((default_[a-z_0-9]*)\)")

    def_value = {}
    for i in range(len(cpp)):
        l = cpp[i]
        m = r.match(l)
        name = ""
        if m:
            name = m.group(2)
            value = m.group(3)
        elif rmin.match(l):
            line = l.strip()
            while line[len(line) - 1] != ';':
                i += 1
                line += cpp[i].strip()
            m = r.match(line)
            if m:
                name = m.group(2)
                value = m.group(3)
        if name != "":
            if value.startswith("anomalydetection::"):
                value = value.replace("anomalydetection::", "action_svc_")
            if value.startswith("hostdependency::"):
                value = value.replace("hostdependency::", "action_hd_")
            if value.startswith("servicedependency::"):
                value = value.replace("servicedependency::", "action_sd_")
            if value.startswith("hostescalation::"):
                value = value.replace("hostescalation::", "action_he_")
            if value.startswith("serviceescalation::"):
                value = value.replace("serviceescalation::", "action_se_")
            if value.startswith("host::"):
                value = value.replace("host::", "action_hst_")
            if value.startswith("service::"):
                value = value.replace("service::", "action_svc_")
            def_value[name] = {
                "name": name,
                "value": value,
            }
    line = 1
    for l in cpp:
        m = rdecl.match(l)
        if m:
            name = proto_name(m.group(1))
            if m.group(2) in def_value:
                value = def_value[m.group(2)]
                for i in msg:
                    if i['proto_name'] == name:
                        i['default'] = value['value']
                        break
            else:
                print(
                    f"Error: {m.group(2)} not found in list of default values from file {filename_cc}")
        line += 1

    if cap_name == "Tag" or cap_name == "Severity":
        for i in msg:
            if i['proto_name'] == "key":
                i['default'] = "0, -1"
                break


def get_correspondence(cpp):
    retval = "{\n"
    r = re.compile(r".*{\"(.*)\",\s*SETTER\([^,]*,\s*_?set_(.*)\)}")

    def_value = {}
    l = ""
    for i in range(len(cpp)):
        prev_l = l.strip()
        l = cpp[i]
        if "SETTER" in l:
            m = r.match(l)
            if not m:
                l = prev_l + l
                m = r.match(l)
                if not m:
                    continue
            key, value = m.group(1), m.group(2)
            if key != value:
                retval += f"    {{\"{key}\", \"{value}\"}},\n"
            l = ""
    retval += "}"
    return retval


def build_hook_content(cname: str, msg):
    retval = []
    els = ""
    if cname == 'Tag':
        retval.append("""
  if (key == "id" || key == "tag_id") {
    uint64_t id;
    if (absl::SimpleAtoi(value, &id)) 
      obj->mutable_key()->set_id(id);
    else
      return false;
    return true;
  } else if (key == "type" || key == "tag_type") {
    if (value == "hostcategory")
      obj->mutable_key()->set_type(tag_hostcategory);
    else if (value == "servicecategory")
      obj->mutable_key()->set_type(tag_servicecategory);
    else if (value == "hostgroup")
      obj->mutable_key()->set_type(tag_hostgroup);
    else if (value == "servicegroup")
      obj->mutable_key()->set_type(tag_servicegroup);
    else
      return false;
    return true;
  }
""")
    elif cname == 'Severity':
        retval.append("""
  if (key == "id" || key == "severity_id") {
    uint64_t id;
    if (absl::SimpleAtoi(value, &id)) 
      obj->mutable_key()->set_id(id);
    else
      return false;
    return true;
  } else if (key == "type" || key == "severity_type") {
    if (value == "host")
      obj->mutable_key()->set_type(severity::host);
    else if (value == "service")
      obj->mutable_key()->set_type(severity::service);
    else
      return false;
    return true;
  }
""")
    elif cname == 'Contact':
        retval.append("""
  if (key == "host_notification_options") {
    uint32_t options;
    if (fill_host_notification_options(&options, value)) {
      obj->set_host_notification_options(options);
      return true;
    }
    else
      return false;
  } else if (key == "service_notification_options") {
    uint32_t options;
    if (fill_service_notification_options(&options, value)) {
      obj->set_service_notification_options(options);
      return true;
    }
    else
      return false;
  }
""")

    for m in msg:
        if m['proto_type'] == 'StringList' or m['proto_type'] == 'StringSet':
            retval.append(f"""  {els}if (key == "{m['proto_name']}") {{
    fill_string_group(obj->mutable_{m['proto_name']}(), value);
    return true;
  }}
""")
        elif m['proto_type'] == 'PairStringSet':
            retval.append(f"""  {els}if (key == "{m['proto_name']}") {{
    fill_pair_string_group(obj->mutable_{m['proto_name']}(), value);
    return true;
  }}
""")
        elif m['proto_type'] == 'repeated PairUint64_32':
            if cname == "Host":
                concerned = "host"
            else:
                concerned = "service"
            retval.append(f"""  {els}if (key == "category_tags") {{
    auto tags{{absl::StrSplit(value, ',')}};
    bool ret = true;

    for (auto it = obj->tags().begin(); it != obj->tags().end(); ) {{
      if (it->second() == TagType::tag_{concerned}category)
        it = obj->mutable_tags()->erase(it);
      else
        ++it;
    }}

    for (auto& tag : tags) {{
      uint64_t id;
      bool parse_ok;
      parse_ok = absl::SimpleAtoi(tag, &id);
      if (parse_ok) {{
        auto t = obj->add_tags();
        t->set_first(id);
        t->set_second(TagType::tag_{concerned}category);
      }} else {{
        ret = false;
      }}
    }}
    return ret;
  }} else if (key == "group_tags") {{
    auto tags{{absl::StrSplit(value, ',')}};
    bool ret = true;

    for (auto it = obj->tags().begin(); it != obj->tags().end(); ) {{
      if (it->second() == TagType::tag_{concerned}group)
        it = obj->mutable_tags()->erase(it);
      else
        ++it;
    }}

    for (auto& tag : tags) {{
      uint64_t id;
      bool parse_ok;
      parse_ok = absl::SimpleAtoi(tag, &id);
      if (parse_ok) {{
        auto t = obj->add_tags();
        t->set_first(id);
        t->set_second(TagType::tag_{concerned}group);
      }} else {{
        ret = false;
      }}
    }}
    return ret;
  }}
""")
        elif m['proto_type'] == 'DaysArray':
            retval.insert(0, """  auto get_timerange = [](const absl::string_view& value, auto* day) -> bool {
    auto arr = absl::StrSplit(value, ',');
    for (auto& d : arr) {
      std::pair<absl::string_view, absl::string_view> v =
          absl::StrSplit(d, '-');
      TimeRange tr;
      std::pair<absl::string_view, absl::string_view> p =
          absl::StrSplit(v.first, ':');
      uint32_t h, m;
      if (!absl::SimpleAtoi(p.first, &h) || !absl::SimpleAtoi(p.second, &m))
        return false;
      tr.set_range_start(h * 3600 + m * 60);
      p = absl::StrSplit(v.second, ':');
      if (!absl::SimpleAtoi(p.first, &h) || !absl::SimpleAtoi(p.second, &m))
        return false;
      tr.set_range_end(h * 3600 + m * 60);
      day->Add(std::move(tr));
    }
    return true;
  };

""")
            retval.append(f"""  {els}if (key == "sunday")
    return get_timerange(value, obj->mutable_timeranges()->mutable_sunday());
  else if (key == "monday")
    return get_timerange(value, obj->mutable_timeranges()->mutable_monday());
  else if (key == "tuesday")
    return get_timerange(value, obj->mutable_timeranges()->mutable_tuesday());
  else if (key == "wednesday")
    return get_timerange(value, obj->mutable_timeranges()->mutable_wednesday());
  else if (key == "thursday")
    return get_timerange(value, obj->mutable_timeranges()->mutable_thursday());
  else if (key == "friday")
    return get_timerange(value, obj->mutable_timeranges()->mutable_friday());
  else if (key == "saturday")
    return get_timerange(value, obj->mutable_timeranges()->mutable_saturday());
""")
        elif m['proto_name'] == 'notification_options':
            retval.append(f"""  {els}if (key == "{m['proto_name']}") {{
    uint16_t options(action_svc_none);
    auto values = absl::StrSplit(value, ',');
    for (auto it = values.begin(); it != values.end(); ++it) {{
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
    }}
    obj->set_notification_options(options);
    return true;
  }}
""")
        elif m['proto_name'] in ['tag_id', 'action_url', 'notes', 'notes_url', 'address', 'alias']:
            pass
        elif m['proto_type'] in ['uint32', 'int32', 'bool', 'uint64', 'int64']:
            pass
        else:
            print(
                f"Field '{m['proto_name']}' of type '{m['proto_type']}' not managed")
        if els == "" and len(retval) > 0:
            els = "else "
    return retval


def build_check_validity(cname: str, msg):
    retval = []
    if cname == "Command":
        retval.append("""
  if (o->command_name().empty())
    throw msg_fmt("Command has no name (property 'command_name')");
  if (o->command_line().empty())
    throw msg_fmt("Command '{}' has no command line (property 'command_line')",
                  o->command_name());
""")
    elif cname == "Contact":
        retval.append("""
      if (o->contact_name().empty())
        throw msg_fmt("Contact has no name (property 'contact_name')");
""")
    elif cname == "Contactgroup":
        retval.append("""
      if (o->contactgroup_name().empty())
        throw msg_fmt("Contactgroup has no name (property 'contactgroup_name')");
""")
    elif cname == "Command":
        retval.append("""
      if (o->command_name().empty())
        throw msg_fmt("Command has no name (property 'command_name')");
      if (o->command_line().empty())
        throw msg_fmt("Command '{}' has no command line (property 'command_line')",
                      o->command_name());
""")
    elif cname == "Connector":
        retval.append("""
  if (o->connector_name().empty())
    throw msg_fmt("Connector has no name (property 'connector_name')");
  if (o->connector_line().empty())
    throw msg_fmt("Connector '{}' has no command line (property 'connector_line')", o->connector_name());
""")
    elif cname == "Host":
        retval.append("""
      if (o->host_name().empty())
        throw msg_fmt("Host has no name (property 'host_name')");
      if (o->address().empty())
        throw msg_fmt("Host '{}' has no address (property 'address')",
                      o->host_name());
""")
    elif cname == "Hostdependency":
        retval.append("""
      if (o->hosts().data().empty() && o->hostgroups().data().empty())
        throw msg_fmt("Host dependency is not attached to any host or host group (properties 'hosts' or 'hostgroups', respectively)");

      if (o->dependent_hosts().data().empty() && o->dependent_hostgroups().data().empty())
        throw msg_fmt(
        "Host dependency is not attached to any "
          "dependent host or dependent host group (properties "
          "'dependent_hosts' or 'dependent_hostgroups', "
          "respectively)");
""")
    elif cname == "Hostescalation":
        retval.append("""
      if (o->hosts().data().empty() && o->hostgroups().data().empty())
        throw msg_fmt("Host escalation is not attached to any host or host group (properties 'hosts' or 'hostgroups', respectively)");
""")
    elif cname == "Hostgroup":
        retval.append("""
      if (o->obj().register_()) {
        if (o->hostgroup_name().empty())
          throw msg_fmt("Host group has no name (property 'hostgroup_name')");
      }
""")
    elif cname == "Service":
        retval.append("""
      if (o->obj().register_()) {
        if (o->service_description().empty())
          throw msg_fmt("Services must have a non-empty description");
        if (o->check_command().empty())
          throw msg_fmt("Service '{}' has an empty check command", o->service_description());
        if (o->hosts().data().empty() && o->hostgroups().data().empty())
          throw msg_fmt("Service '{}' must contain at least one of the fields 'hosts' or 'hostgroups' not empty", o->service_description());
      }
""")
    elif cname == "Servicedependency":
        retval.append("""
      /* Check base service(s). */
      if (o->servicegroups().data().empty()) {
        if (o->service_description().data().empty())
          throw msg_fmt("Service dependency is not attached to any service or service group (properties 'service_description' or 'servicegroup_name', respectively)");
        else if (o->hosts().data().empty() && o->hostgroups().data().empty())
          throw msg_fmt("Service dependency is not attached to any host or host group (properties 'host_name' or 'hostgroup_name', respectively)");
      }

      /* Check dependent service(s). */
  if (o->dependent_servicegroups().data().empty()) {
    if (o->dependent_service_description().data().empty())
      throw msg_fmt(
          "Service dependency is not attached to "
                          "any dependent service or dependent service group "
                          "(properties 'dependent_service_description' or "
                          "'dependent_servicegroup_name', respectively)");
    else if (o->dependent_hosts().data().empty() && o->dependent_hostgroups().data().empty())
      throw msg_fmt(
            "Service dependency is not attached to "
            "any dependent host or dependent host group (properties "
            "'dependent_host_name' or 'dependent_hostgroup_name', "
            "respectively)");
  }
""")
    elif cname == "Servicegroup":
        retval.append("""
      if (o->servicegroup_name().empty())
        throw msg_fmt("Service group has no name (property 'servicegroup_name')");
""")
    elif cname == "Timeperiod":
        retval.append("""
      if (o->timeperiod_name().empty())
        throw msg_fmt("Time period has no name (property 'timeperiod_name')");
""")
    elif cname == "Anomalydetection":
        retval.append("""
      if (o->obj().register_()) {
        if (o->service_description().empty())
          throw msg_fmt("Anomaly detection has no name (property 'service_description')");
        if (o->host_name().empty())
          throw msg_fmt("Anomaly detection '{}' has no host name (property 'host_name')", o->service_description());
        if (o->metric_name().empty())
          throw msg_fmt("Anomaly detection '{}' has no metric name (property 'metric_name')", o->service_description());
        if (o->thresholds_file().empty())
          throw msg_fmt("Anomaly detection '{}' has no thresholds file (property 'thresholds_file')", o->service_description());
      }
""")
    elif cname == "Tag":
        retval.append("""
      if (o->tag_name().empty())
        throw msg_fmt("Tag has no name (property 'tag_name')");
      if (o->key().id() == 0)
        throw msg_fmt("Tag '{}' has a null id", o->tag_name());
      if (o->key().type() == static_cast<uint32_t>(-1))
        throw msg_fmt("Tag type must be specified");
""")
    elif cname == "Severity":
        retval.append("""
      if (o->severity_name().empty())
        throw msg_fmt("Severity has no name (property 'severity_name')");
      if (o->key().id() == 0)
        throw msg_fmt(
            "Severity id must not be less than 1 (property 'severity_id')");
      if (o->level() == 0)
        throw msg_fmt(
            "Severity level must not be less than 1 (property 'level')");
      if (o->key().type() == severity::none)
        throw msg_fmt("Severity type must be one of 'service' or 'host'");
""")
    elif cname == "Servicegroup":
        retval.append("""
      if (o->servicegroup_name().empty())
        throw msg_fmt("Service group has no name (property 'servicegroup_name')");
""")
    elif cname == "Serviceescalation":
        retval.append("""
  if (o->servicegroups().data().empty()) {
    if (o->service_description().data().empty())
      throw msg_fmt("Service escalation is not attached to "
                            "any service or service group (properties "
                            "'service_description' and 'servicegroup_name', "
                            "respectively)");
    else if (o->hosts().data().empty() && o->hostgroups().data().empty())
      throw
          msg_fmt("Service escalation is not attached to "
                          "any host or host group (properties 'host_name' or "
                          "'hostgroup_name', respectively)");
  }
""")
    else:
        print(f"### ERROR!!!: {cname} has no function check_validity()")
    return retval


def proto_name(n: str):
    return n[1:]


proto = ["""/*
** Copyright 2022 Centreon
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
**
** For more information : contact@centreon.com
*/

syntax = "proto3";

package com.centreon.engine.configuration;

message CustomVariable {
  string name = 1;
  string value = 2;
  bool is_sent = 3;
  bool modified = 4;
}

message Object {
  string name = 1;
  bool register = 2;
  repeated string use = 3;
}

message Point2d {
  int32 x = 1;
  int32 y = 2;
}

message Point3d {
  double x = 1;
  double y = 2;
  double z = 3;
}

message KeyType {
  uint64 id = 1;
  uint32 type = 2;
}

message DaysArray {
  repeated TimeRange sunday = 1;
  repeated TimeRange monday = 2;
  repeated TimeRange tuesday = 3;
  repeated TimeRange wednesday = 4;
  repeated TimeRange thursday = 5;
  repeated TimeRange friday = 6;
  repeated TimeRange saturday = 7;
}

message TimeRange {
  uint64 range_start = 1;
  uint64 range_end = 2;
}

message Daterange {
  enum TypeRange {
    calendar_date = 0;
    month_date = 1;
    month_day = 2;
    month_week_day = 3;
    week_day = 4;
    none = 5; // Instead of -1 in original config
  }
  TypeRange type = 1;
  int32 syear = 2;  // Start year.
  int32 smon = 3;   // Start month.
  // Start day of month (may 3rd, last day in feb).
  int32 smday = 4;
  int32 swday = 5;  // Start day of week (thursday).
  // Start weekday offset (3rd thursday, last monday in jan).
  int32 swday_offset = 6;
  int32 eyear = 7;
  int32 emon = 8;
  int32 emday = 9;
  int32 ewday = 10;
  int32 ewday_offset = 11;
  int32 skip_interval = 12;
  repeated TimeRange timerange = 13;
}

message ExceptionArray {
  repeated Daterange calendar_date = 1;
  repeated Daterange month_date = 2;
  repeated Daterange month_day = 3;
  repeated Daterange month_week_day = 4;
  repeated Daterange week_day = 5;
}

message PairStringSet {
  message Pair {
    string first = 1;
    string second = 2;
  }
  repeated Pair data = 1;
  bool additive = 2;
}

message PairUint64_32 {
  uint64 first = 1;
  uint32 second = 2;
}

enum DependencyKind {
  unknown = 0;
  notification_dependency = 1;
  execution_dependency = 2;
}

enum ActionServiceOn {
  action_svc_none = 0;
  action_svc_ok = 1;                // (1 << 0)
  action_svc_warning = 2;           // (1 << 1)
  action_svc_unknown = 4;           // (1 << 2)
  action_svc_critical = 8;          // (1 << 3)
  action_svc_flapping = 16;         // (1 << 4)
  action_svc_downtime = 32;         // (1 << 5)
}

enum ActionHostOn {
  action_hst_none = 0;
  action_hst_up = 1;                // (1 << 0)
  action_hst_down = 2;              // (1 << 1)
  action_hst_unreachable = 4;       // (1 << 2)
  action_hst_flapping = 8;          // (1 << 3)
  action_hst_downtime = 16;         // (1 << 4)
}

enum ActionHostEscalationOn {
  action_he_none = 0;
  action_he_down = 1;                     // (1 << 0)
  action_he_unreachable = 2;              // (1 << 1)
  action_he_recovery = 4;                 // (1 << 2)
}

enum ActionServiceEscalationOn {
    action_se_none = 0;
    action_se_unknown = 1;            // (1 << 1)
    action_se_warning = 2;            // (1 << 2)
    action_se_critical = 4;           // (1 << 3)
    action_se_pending = 8;            // (1 << 4)
    action_se_recovery = 16;          // (1 << 5)
}

enum ActionServiceDependencyOn {
  action_sd_none = 0;
  action_sd_ok = 1;               // (1 << 0)
  action_sd_unknown = 2;          // (1 << 1)
  action_sd_warning = 4;          // (1 << 2)
  action_sd_critical = 8;         // (1 << 3)
  action_sd_pending = 16;         // (1 << 4)
}
 
enum ActionHostDependencyOn {
    action_hd_none = 0;
    action_hd_up = 1;             // (1 << 0)
    action_hd_down = 2;           // (1 << 1)
    action_hd_unreachable = 4;    // (1 << 2)
    action_hd_pending = 8;        // (1 << 3)
}
 
enum TagType {
    tag_servicegroup = 0;
    tag_hostgroup = 1;
    tag_servicecategory = 2;
    tag_hostcategory = 3;
    tag_none = 255;         // in legacy configuration, this was -1
}

message StringList {
    repeated string data = 1;
    bool additive = 2;
}

message StringSet {
    repeated string data = 1;
    bool additive = 2;
}

"""]

for i in range(len(class_files_hh)):
    # Time to read cc/hh files for each configuration object
    filename_hh = class_files_hh[i]
    filename_cc = class_files_cc[i]
    filehelper_cc = filename_cc.replace(
        ".cc", "-helper.cc").replace("src/configuration/", "")
    filehelper_hh = filename_hh.replace(
        ".hh", "-helper.hh").replace("inc/com/centreon/engine/configuration/", "")
    name = filename_hh.split('/')[-1]
    name = name.split('.')[0]
    cap_name = name.capitalize()

    fhcc = prepare_filehelper_cc(name)
    fhhh = prepare_filehelper_hh(cap_name, name)

    f = open(f"../engine/{filename_hh}", 'r')
    header = f.readlines()
    f.close()

    f = open(f"../engine/{filename_cc}", 'r')
    cpp = f.readlines()
    f.close()

    ### Parsing the two files ###
    msg_list = []

    # From the header file, we get fields with their type.
    get_messages_of_conf(header, msg_list)

    # From the cpp sources, we get default values for fields.
    get_default_values(cap_name, cpp, msg_list)

    # Construction of the hook for this message i.e. all the particular cases.
    hook = build_hook_content(cap_name, msg_list)

    # Construction of the check_validity for this message.
    check_validity = build_check_validity(cap_name, msg_list)

    # From the cpp sources, we get the correspondence.
    correspondence = get_correspondence(cpp)

    # Construction of three files, state-generated.proto, state-generated.cc and state-generated.hh
    number = 2
    proto.append(f"""
message {cap_name} {{
  Object obj = 1;
""")
    for l in msg_list:
        cmt = ""
        if l['optional']:
            cmt += "Optional"
            optional = "optional "
        else:
            cmt += ""
            optional = ""
        if 'default' in l:
            cmt += f" - Default value: {l['default']}"
        if cmt != "":
            cmt = "  // " + cmt
        proto.append(
            f"  {optional}{l['proto_type']} {l['proto_name']} = {number};{cmt}\n")
        number += 1
    proto.append("}\n")

    complete_filehelper_cc(fhcc, cap_name, name, number,
                           correspondence, hook, check_validity, msg_list)

    # Generation of proto file
    f = open("/tmp/configuration/state-generated.proto", "w")
    f.writelines(proto)
    f.close()

    fhcc.close()
    fhhh.close()


indent_files()
sync_files()
