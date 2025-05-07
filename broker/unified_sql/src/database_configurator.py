#!/usr/bin/env python3
#
# Copyright 2025 Centreon
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# For more information : contact@centreon.com
import re
import sys
from collections import namedtuple

Field = namedtuple("DBData", "field field_type col_name col_type options")
datatype = {
    "int32": "i32",
    "uint32": "u32",
    "int64": "i64",
    "uint64": "u64",
    "string": "str",
    "bool": "bool",
    "float": "f32",
    "double": "f64",
}

if len(sys.argv) != 2:
    print("Usage: python generate_functions.py <database_configurator.cc.in>")
    sys.exit(1)

file_in = sys.argv[1]

if not file_in.endswith('.in'):
    print("Error: The input file must have a .in extension.")
    sys.exit(1)

file_out = file_in[:-6] + ".cc"

try:
    with open(file_in, 'r') as f:
        lines = f.readlines()
except FileNotFoundError:
    print(f"Error: The file {file_in} does not exist.")
    sys.exit(1)

# Regex patterns to extract information
r_start = re.compile(r"\s*/\*\* Database configuration")
r_query = re.compile(r"\s*\*\s*Query\s*:\s*(.*)")
r_description = re.compile(r"\s*\*\s*Description\s*:\s*(.*)")
r_return = re.compile(r"\s*\*\s*Return\s*:\s*(.*)")
r_key = re.compile(r"\s*\*\s*Key\s*:\s*(.*)")
r_method = re.compile(r"\s*\*\s*Method\s*:\s*(.*)")
r_pb_msg = re.compile(r"\s*\*\s*Protobuf message\s*:\s*([^\s]+)")
r_table = re.compile(r"\s*\*\s*Table\s*:\s*(.*)")
r_field = re.compile(
    r"\s*\*\s*(\${[^}]+}|[\w:]+)\s*&\s*(\w+)\s*&\s*(\w+)\s*&\s*(\w+)\s*&\s*(\w*)")
r_value = re.compile(r"\${([^}]+)}")
r_options = re.compile(r"^[UOA]*$")
in_block = False
fields = []
query = ""

# Generate the _add_{table}_mariadb function


def key_generator(table, key_value):
    r_pair = re.compile(
        r"{\s*([a-zA-Z0-9_:${}]+)\s*,\s*\s*([a-zA-Z0-9_:${}]+)\s*}")
    r_simple_key = re.compile(r"([\w:]+)")
    r_value = re.compile(r"\${([^}]+)}")
    m_pair = r_pair.match(key_value)
    m_simple_key = r_simple_key.match(key_value)
    if m_pair:
        key1 = m_pair.group(1)
        key2 = m_pair.group(2)
        if ":" in key1:
            key1 = key1.split("::")
            key1 = f"msg.{key1[0]}().{key1[1]}()"
        else:
            m_value = r_value.match(key1)
            if m_value:
                key1 = m_value.group(1)
            else:
                key1 = f"msg.{key1}()"
        if ":" in key2:
            key2 = key2.split("::")
            key2 = f"msg.{key2[0]}().{key2[1]}()"
        else:
            m_value = r_value.match(key2)
            if m_value:
                key2 = m_value.group(1)
            else:
                key2 = f"msg.{key2}()"
        retval = f"std::make_pair({key1}, {key2})"
    elif m_simple_key:
        key = m_simple_key.group(1)
        if ":" in key:
            key = key.split(":")
            key = f"msg.{key[0]}().{key[1]}()"
        else:
            m_value = r_value.match(key)
            if m_value:
                key = m_value.group(1)
            else:
                key = f"{key}()"
        retval = key
    else:
        print(
            f"To return some cache, we must know how to build keys (concerning table '{table}')", file=sys.stderr)
        exit(4)
    return retval


def generate_query_mariadb(query, method, return_value, key_value, pb_msg, table, fields):
    if query == "INSERT ON DUPLICATE KEY UPDATE":
        query_fields = ",".join(
            f"{field.col_name}" for field in fields if 'A' not in field.options)
        query_prefix = f"INSERT INTO {table} ({query_fields}) VALUES"
        query_suffix = "ON DUPLICATE KEY UPDATE "
        query_suffix += ",".join(
            f"{field.col_name}=VALUES({field.col_name})"
            for field in fields if 'U' not in field.options
        )
    else:
        print("Error: Unsupported query type.", file=sys.stderr)
        sys.exit(1)

    id_to_get = False

    params = ",".join("?" for field in fields if 'A' not in field.options)
    binds = []
    idx = 0
    for field in fields:
        if 'A' in field.options:
            id_to_get = True
            continue
        if field.col_type == "string":
            m_value = r_value.match(field.field)
            if not m_value:
                if 'O' in field.options:
                    binds.append(f"""    if (msg.has_{field.field}())
          bind->set_value_as_{datatype[field.col_type]}({idx}, common::truncate_utf8(msg.{field.field}(), get_centreon_storage_{table}_col_size(centreon_storage_{table}_{field.col_name})));
    else
      bind->set_null_{datatype[field.col_type]}({idx});"""
                                 )
                else:
                    binds.append(
                        f'    bind->set_value_as_{datatype[field.col_type]}({idx}, common::truncate_utf8(msg.{field.field}(), get_centreon_storage_{table}_col_size(centreon_storage_{table}_{field.col_name})));')
            else:
                if m_value.group(1) == "NULL":
                    binds.append(
                        f'    bind->set_null_{datatype[field.col_type]}({idx});')
                else:
                    binds.append(
                        f'    bind->set_value_as_{datatype[field.col_type]}({idx}, common::truncate_utf8({field.field[2:-1]}, get_centreon_storage_{table}_col_size(centreon_storage_{table}_{field.col_name})));')
        else:
            m_value = r_value.match(field.field)
            if not m_value:
                if "::" in field.field:
                    arr = field.field.split("::")
                    has_field = f"{arr[0]}().has_{arr[1]}()"
                    my_field = f"{arr[0]}().{arr[1]}()"
                else:
                    has_field = f"has_{field.field}()"
                    my_field = f"{field.field}()"
                if 'O' in field.options:
                    binds.append(f"""    if (msg.{has_field})
      bind->set_value_as_{datatype[field.col_type]}({idx}, msg.{my_field});
    else
      bind->set_null_{datatype[field.col_type]}({idx});"""
                                 )
                else:
                    binds.append(
                        f'    bind->set_value_as_{datatype[field.col_type]}({idx}, msg.{my_field});')
            else:
                if m_value.group(1) == "NULL":
                    binds.append(
                        f'    bind->set_null_{datatype[field.col_type]}({idx});')
                else:
                    binds.append(
                        f'    bind->set_value_as_{datatype[field.col_type]}({idx}, {field.field[2:-1]});')
        idx += 1
    bind_code = "\n".join(binds)
    offset_code = ""

    if id_to_get:
        r_key_type = re.compile(r".*flat_hash_map<(.*),[^,]+>\s*")
        m_key_type = r_key_type.match(return_value)
        cache_decl = f", {return_value}& cache"
        cache_decl_ext = f"""std::list<{m_key_type.group(1)}> keys;"""
        offset_code = f"""    auto key = {key_generator(table, key_value)};
    keys.push_back(key);
"""

        query = """
  try {
    std::promise<uint64_t> promise;
    std::future<uint64_t> future = promise.get_future();
    mysql.run_statement_and_get_int<uint64_t>(stmt, std::move(promise), mysql_task::int_type::LAST_INSERT_ID);
    int first_id = future.get();
    for (auto& k : keys)
      cache[k] = first_id++;
  } catch (const std::exception& e) {
      _logger->error("Error while executing <<{{}}>>: {{}}", query, e.what());
  }"""
    else:
        query = "mysql.run_statement(stmt);"
        cache_decl = ""
        cache_decl_ext = ""

    return f"""/**
 * @brief {description} (code for MariaDB).
 *
 * @param lst The list of messages to add/update.
 */
void database_configurator::{method}_mariadb(const ::google::protobuf::RepeatedPtrField<{pb_msg}>& lst{cache_decl}) {{
  {cache_decl_ext}
  std::string query("{query_prefix} ({params}) {query_suffix}");
  mysql_bulk_stmt stmt(query);
  mysql& mysql = _stream->get_mysql();
  if (!stmt.prepared())
    mysql.prepare_statement(stmt);

  auto bind = stmt.create_bind();

  for (const auto& msg : lst) {{
{offset_code}
{bind_code}
    bind->next_row();
  }}
  stmt.set_bind(std::move(bind));
{query}
}}
"""

# Generate the _add_hosts_mysql function


def generate_query_mysql(query, method, return_value, key_value, pb_msg, table, fields):
    if query == "INSERT ON DUPLICATE KEY UPDATE":
        query_prefix = f"INSERT INTO {table} VALUES"
        query_suffix = "ON DUPLICATE KEY UPDATE "
        query_suffix += ",".join(
            f"{field.col_name}=VALUES({field.col_name})"
            for field in fields if 'U' not in field.options
        )
    else:
        print("Error: Unsupported query type.", file=sys.stderr)
        sys.exit(2)

    id_to_get = False
    insertor = []
    values = []
    for field in fields:
        m_value = r_value.match(field.field)
        if 'A' in field.options:
            id_to_get = True
            continue
        if m_value:
            if m_value.group(1) == "NULL":
                insertor.append("NULL")
            elif field.field_type == 'bool':
                b = m_value.group(1)
                if b == "true":
                    insertor.append("1")
                elif b == "false":
                    insertor.append("0")
                else:
                    values.append(f"{m_value.group(1)} ? 1 : 0")
                    insertor.append("{}")
            elif field.field_type == "string":
                values.append(
                    f"misc::string::escape({m_value.group(1)}, get_centreon_storage_{table}_col_size(centreon_storage_{table}_{field.col_name}))")
                insertor.append("'{}'")
            else:
                values.append(m_value.group(1))
                insertor.append("{}")
        else:
            if "::" in field.field:
                arr = field.field.split("::")
                has_field = f"{arr[0]}().has_{arr[1]}()"
                my_field = f"{arr[0]}().{arr[1]}()"
            else:
                has_field = f"has_{field.field}()"
                my_field = f"{field.field}()"
            if field.field_type == "string":
                if 'O' in field.options:
                    values.append(
                        f"msg.{has_field} ? misc::string::escape(msg.{my_field}, get_centreon_storage_{table}_col_size(centreon_storage_{table}_{field.col_name})) : NULL")
                    insertor.append("'{}'")
                else:
                    values.append(
                        f"misc::string::escape(msg.{my_field}, get_centreon_storage_{table}_col_size(centreon_storage_{table}_{field.col_name}))")
                    insertor.append("'{}'")
            else:
                values.append(f"msg.{my_field}")
                insertor.append("{}")
    values_str = ",".join(insertor)
    values_code = ", ".join(values)
    generate_key_offset = ""

    if id_to_get:
        r_key_type = re.compile(r".*flat_hash_map<(.*),[^,]+>\s*")
        m_key_type = r_key_type.match(return_value)
        cache_decl = f", {return_value}& cache"
        cache_decl_ext = f"""std::list<{m_key_type.group(1)}> keys;"""
        key = key_generator(table, key_value)
        generate_key_offset = f"""auto key = {key};
    keys.push_back(key);
"""
        query = """
  try {
    std::promise<int> promise;
    std::future<int> future = promise.get_future();
    mysql.run_query_and_get_int(query, std::move(promise), mysql_task::int_type::LAST_INSERT_ID);
    int first_id = future.get();
    for (auto& k : keys)
      cache[k] = first_id++;
  } catch (const std::exception& e) {
    _logger->error("Error while executing <<{{}}>>: {{}}", query, e.what());
  }
"""
    else:
        query = "mysql.run_query(query);"
        cache_decl = ""
        cache_decl_ext = ""

    return f"""

/**
 * @brief {description} (code for MySQL).
 *
 * @param lst The list of messages to add/update.
 */
void database_configurator::{method}_mysql(const ::google::protobuf::RepeatedPtrField<{pb_msg}>& lst{cache_decl}) {{
  mysql& mysql = _stream->get_mysql();
  {cache_decl_ext}

  std::vector<std::string> values;
  for (const auto& msg : lst) {{
    {generate_key_offset}
    std::string value(
        fmt::format("({values_str})", {values_code}));
    values.emplace_back(value);
  }}
  std::string query(fmt::format("{query_prefix} {{}} {query_suffix}", fmt::join(values, ",")));
{query}
}}
"""


# Process the file and generate the output
output_lines = []
return_value = "void"
key_value = ""

for line in lines:
    output_lines.append(line)
    if not in_block:
        if r_start.match(line):
            in_block = True
            fields = []
            query = ""
    else:
        m_query = r_query.match(line)
        m_description = r_description.match(line)
        m_method = r_method.match(line)
        m_return = r_return.match(line)
        m_key = r_key.match(line)
        m_field = r_field.match(line)
        m_pb_msg = r_pb_msg.match(line)
        m_table = r_table.match(line)
        if m_query:
            query = m_query.group(1).strip()
        elif m_description:
            description = m_description.group(1).strip()
            if not description:
                print("Error: Description is required.", file=sys.stderr)
                sys.exit(2)
        elif m_method:
            method = m_method.group(1).strip()
        elif m_return:
            return_value = m_return.group(1).strip()
        elif m_key:
            key_value = m_key.group(1).strip()
        elif m_table:
            table = m_table.group(1).strip()
        elif m_pb_msg:
            pb_msg = m_pb_msg.group(1).strip()
        elif m_field:
            if not r_options.match(m_field.group(5)):
                print(f"""Error: Invalid options in row:\n{m_field.group(0)}:\n Only 'O', 'A' and 'U' are authorized:
  -O: to specify that the field is optional (in the database it can be NULL)
  -U: to specify that the field is a unique key, in an update it is not changed.
  -A: to specify that the field is an auto increment field, it does not appear in the insert query.
  """, file=sys.stderr)
                sys.exit(3)

            fields.append(Field(field=m_field.group(1), field_type=m_field.group(
                2), col_name=m_field.group(3), col_type=m_field.group(4), options=m_field.group(5)))
        elif line.strip() == "*/":
            in_block = False
            # Check if we have all the required informations
            if not (query and method and pb_msg and table and fields):
                print(
                    "Error: Missing required information in the comment block.", file=sys.stderr)
                sys.exit(4)
            if any('A' in field.options for field in fields) and return_value == 'void':
                print(
                    f"Error: Missing return value in the comment block for table '{table}'.", file=sys.stderr)
                sys.exit(5)
            if return_value != 'void' and all('A' not in field.options for field in fields):
                print(
                    f"Error: Return value is only allowed for auto increment fields (table '{table}').", file=sys.stderr)
                sys.exit(6)
            # Generate and insert the functions after the comment block
            output_lines.append(generate_query_mariadb(
                query, method, return_value, key_value, pb_msg, table, fields))
            output_lines.append("\n")
            output_lines.append(generate_query_mysql(
                query, method, return_value, key_value, pb_msg, table, fields))
            output_lines.append("\n")
            # Reset the variables for the next block
            return_value = "void"
            key_value = ""
            descrption = ""

# Write the output to the new file
with open(file_out, 'w') as f:
    f.writelines(output_lines)

print(f"Generated file written to {file_out}")
