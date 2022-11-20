#!/usr/bin/env python3.9
import re

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
    "inc/com/centreon/engine/configuration/hostextinfo.hh",
    "inc/com/centreon/engine/configuration/service.hh",
    "inc/com/centreon/engine/configuration/servicedependency.hh",
    "inc/com/centreon/engine/configuration/serviceescalation.hh",
    "inc/com/centreon/engine/configuration/serviceextinfo.hh",
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
    "src/configuration/hostextinfo.cc",
    "src/configuration/service.cc",
    "src/configuration/servicedependency.cc",
    "src/configuration/serviceescalation.cc",
    "src/configuration/serviceextinfo.cc",
    "src/configuration/servicegroup.cc",
    "src/configuration/severity.cc",
    "src/configuration/tag.cc",
    "src/configuration/timeperiod.cc",
]


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
        "group<set_string>": "repeated string",
        "group<list_string>": "repeated string",
        "point_2d": "Point2d",
        "point_3d": "Point3d",
        "dependency_kind": "DependencyKind",
        "key_type": "KeyType",
        "days_array": "DaysArray",
        "tab_string": "repeated string",
        "exception_array": "ExceptionArray",
        "group<set_pair_string>": "repeated PairString",
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


def get_default_values(cpp, msg: [str]):
    rmin = re.compile(r"(?:static)?\s*([a-z0-9][a-z_0-9\s]+[a-z0-9])\s*(?:const)?\s*default_")
    r = re.compile(r"(?:static)?\s*([a-z0-9][a-z_0-9\s]+[a-z0-9])\s*(?:const)?\s*(default_[a-z0-9_]*)\((.*)\);")
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
                print(f"Error: {m.group(2)} not found in list of default values from file {filename_cc}")
        line += 1


def proto_name(n : str):
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

message PairString {
  string first = 1;
  string second = 2;
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
 
"""]

fcc = open("state-generated.cc", "w")
fcc.write("""/*
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
#include "state-generated.hh"
#include "com/centreon/engine/host.hh"
#include "com/centreon/engine/service.hh"

namespace com::centreon::engine::configuration {
""")


fhh = open("state-generated.hh", "w")
fhh.write("""/*
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

#ifndef CCE_CONFIGURATION_STATE_GENERATED_HH
#define CCE_CONFIGURATION_STATE_GENERATED_HH

#include "state-generated.pb.h"

namespace com {
namespace centreon {
namespace engine {
namespace configuration {
""")

for i in range(len(class_files_hh)):
    # Time to read cc/hh files for each configuration object
    filename_hh = class_files_hh[i]
    filename_cc = class_files_cc[i]
    name = filename_hh.split('/')[-1]
    name = name.split('.')[0]
    cap_name = name.capitalize()

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

    # From the cpp sources, we get default values for some of the fields.
    get_default_values(cpp, msg_list)


    ### Construction of three files, state-generated.proto, state-generated.cc and state-generated.hh
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
        proto.append(f"  {optional}{l['proto_type']} {l['proto_name']} = {number};{cmt}\n")
        number += 1
    proto.append("}\n")


    # Generation of proto file
    f = open("state-generated.proto", "w")
    f.writelines(proto)
    f.close()

    #Generation of cpp file
    cc_lines =[f"\n{cap_name} make_{name}() {{\n  {cap_name} retval;\n"]
    for m in msg_list:
        if 'default' in m:
            if m['typ'] == 'point_2d':
                values = m['default'].split(',')
                cc_lines.append(f"  retval.mutable_{m['proto_name']}()->set_x({values[0].strip()});\n")
                cc_lines.append(f"  retval.mutable_{m['proto_name']}()->set_y({values[1].strip()});\n")
            elif m['typ'] == 'point_3d':
                values = m['default'].split(',')
                cc_lines.append(f"  retval.mutable_{m['proto_name']}()->set_x({values[0].strip()});\n")
                cc_lines.append(f"  retval.mutable_{m['proto_name']}()->set_y({values[1].strip()});\n")
                cc_lines.append(f"  retval.mutable_{m['proto_name']}()->set_y({values[2].strip()});\n")
            else:
                cc_lines.append(f"  retval.set_{m['proto_name']}({m['default']});\n")

    cc_lines.append("  return retval;\n}\n")
    fcc.writelines(cc_lines)

    #Generation of hh file
    fhh.write(f"{cap_name} make_{name}();\n")

fhh.write("""
};  // namespace configuration
};  // namespace engine
};  // namespace centreon
};  // namespace com

#endif /* !CCE_CONFIGURATION_STATE_GENERATED_HH */
""")
fcc.write("}; // com::centreon::engine::configuration\n")
fcc.close()
fhh.close()
