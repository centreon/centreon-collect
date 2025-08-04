#!/usr/bin/python3
"""
* Copyright 2019-2020 Centreon
*
* Licensed under the Apache License, Version 2.0(the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
*     http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*
* For more information : contact@centreon.com
"""

import sys
import re
from datetime import datetime
import os

dico = {}
pattern_ct = re.compile(
    r'CREATE TABLE( IF NOT EXISTS)? (centreon_storage\.)?`?([^`]*)`? \(')
column_ct = re.compile(r'\s*`?([^`]*)`? (varchar\(([0-9]*)\)|text)')
column_ct_nb = re.compile(r'\s*`?([^`]*)`? (float|double)')
end_ct = re.compile(r'^\)')

debug = False


def print_dbg(s):
    if debug:
        print(s)


header_file = sys.argv[1]
for sql_file in sys.argv[2:]:
    with open(sql_file, encoding='utf-8') as fp:
        database = os.path.basename(sql_file).split('.')[0]
        line = fp.readline()
        in_block = False

        while line:
            if not in_block:
                m = pattern_ct.match(line)
                if m:
                    print_dbg("New table {}".format(m.group(3)))
                    current_table = f"{database}_{m.group(3)}"
                    in_block = True
            else:
                if end_ct.match(line):
                    in_block = False
                else:
                    m = column_ct.match(line)
                    if m:
                        if m.group(3):
                            print_dbg("New text column {} of size {}".format(
                                m.group(1), m.group(3)))
                            dico.setdefault(current_table, []).append(
                                (m.group(1), 'TEXT', m.group(3)))
                        else:
                            print_dbg("New text column {} of 65534".format(
                                m.group(1), m.group(2)))
                            dico.setdefault(current_table, []).append(
                                (m.group(1), 'TEXT', 65534))
                    else:
                        m = column_ct_nb.match(line)
                        if m:
                            print_dbg(f"New number column {m.group(1)} of type {m.group(2)}")
                            dico.setdefault(current_table, []).append(
                                (m.group(1), 'NUMBER', m.group(2)))
    
            line = fp.readline()

        fp.close()

cols = ""

for t, content in dico.items():
    cols += "enum {}_cols {{\n".format(t)
    sizes = "constexpr static pair_type_size {}_size[] {{\n".format(t)
    for c in content:
        cols += f"  {t}_{c[0]},\n"
        if c[1] == 'TEXT':
            sizes += f"    {{{c[1]}, {c[2]}}},  // {c[0]}\n"
        else:
            if c[2] == 'double':
                s = 8
            else:
                s = 4
            sizes += f"    {{{c[1]}, {s}}},  // {c[0]}\n"
    cols += "};\n"
    sizes += "};\n"
    cols += sizes

    cols += f"""constexpr uint32_t get_{t}_col_size(
    {t}_cols const& col) {{
  return {t}_size[col].size;
}}

constexpr col_type get_{t}_col_type(
    {t}_cols const& col) {{
  return {t}_size[col].type;
}}

"""

with open(header_file, 'w', encoding="utf-8") as fp:
    year = datetime.now().year
    fp.write(f"""/**
 * Copyright 2020-{year} Centreon
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * For more information : contact@centreon.com
 */

#ifndef __TABLE_MAX_SIZE_HH__
#define __TABLE_MAX_SIZE_HH__

namespace com::centreon::broker {{

enum col_type {{
  TEXT,
  NUMBER
}};

struct pair_type_size {{
  col_type type;
  uint32_t size;
}};

""")

    fp.write(cols)
    fp.write("\n\n}\n\n#endif /* __TABLE_MAX_SIZE_HH__ */")
