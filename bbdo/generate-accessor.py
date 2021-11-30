#!/usr/bin/python3
"""
** Copyright 2021 Centreon
**
** Licensed under the Apache License, Version 2.0(the "License");
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
"""

import sys
import re

obj = sys.argv[1]
obj_u = obj.upper()
obj_c = obj.capitalize()

accessors = []
block_level = 0
decl = re.compile(r'\s*([a-zA-Z0-9]+)\s+([a-z_0-9]+)\s*=\s*[0-9]+;')
enum = re.compile(r'\s*enum\s+([a-zA-Z]+)\s+{')
enums = []
with open("{}.proto".format(obj), encoding='utf-8') as fp:
    line = fp.readline()
    while line:
        if block_level == 0:
            if line == "message {} {{\n".format(obj_c):
                block_level += 1
        else:
            if line == "}":
                block_level -= 1
            else:
                if block_level == 1:
                    m = decl.match(line)
                    if m:
                        if m.group(1) in ['bool', 'int32', 'int64', 'uint32', 'uint64', 'string'] or\
                           m.group(1) in enums:
                            d = """    {{"{0}", [obj = this->_obj] {{ return misc::variant(obj->{0}()); }}}}""".format(
                                m.group(2))
                            accessors.append(d)
                    else:
                        m = enum.match(line)
                        if m:
                            enums.append(m.group(1))
        line = fp.readline()
    fp.close()

acc = ",\n".join(accessors)
content = """#ifndef CC_BROKER_{0}_ACCESSOR_HH
#define CC_BROKER_{0}_ACCESSOR_HH
#include <unordered_map>
#include "com/centreon/broker/misc/variant.hh"
#include "com/centreon/exceptions/msg_fmt.hh"
#include "bbdo/{2}.pb.h"

namespace com {{
namespace centreon {{
namespace broker {{

class {2}_accessor {{
  {1}* const _obj;
  const std::unordered_map<std::string, std::function<misc::variant()>> _get{{
{3}
  }};

 public:
  {2}_accessor({1}* obj) : _obj{{obj}} {{}}
  misc::variant get(const std::string& key) {{
    auto it = _get.find(key);
    if (it != _get.end())
      return it->second();
    else
      throw exceptions::msg_fmt(
        "Service protobuf object does not contain '{{}}' element", key);
  }}
}};

}}  // namespace broker
}}  // namespace centreon
}}  // namespace com

#endif /* !CC_BROKER_{0}_ACCESSOR_HH */
""".format(obj_u, obj_c, obj, acc)

print(content, file=open("{}_accessor.hh".format(obj), "w"))
