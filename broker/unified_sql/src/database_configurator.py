#!/usr/bin/env python3
import re
import sys
from collections import namedtuple
from typing import NamedTuple

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
r_pb_msg = re.compile(r"\s*\*\s*Protobuf message\s*:\s*([^\s]+)")
r_table = re.compile(r"\s*\*\s*Table\s*:\s*(.*)")
r_field = re.compile(r"\s*\*\s*(\${[^}]+}|\w+)\s*&\s*(\w+)\s*&\s*(\w+)\s*&\s*(\w+)\s*&\s*(\w*)")
r_value = re.compile(r"\${([^}]+)}")
r_options = re.compile(r"^[UO]*$")
in_block = False
fields = []
query = ""

# Generate the _add_hosts_mariadb function
def generate_query_mariadb(query, pb_msg, table, fields):
    if query == "INSERT ON DUPLICATE KEY UPDATE":
        query_prefix = f"INSERT INTO {table} VALUES "
        query_suffix = "ON DUPLICATE KEY UPDATE "
        query_suffix += ",".join(
            f"{field.col_name} = VALUES({field.col_name})"
            for field in fields if not 'U' in field.options
        )
    else:
        print("Error: Unsupported query type.", file=sys.stderr)
        sys.exit(1)
    params = ", ".join("?" for _ in fields)
    binds = []
    for i, field in enumerate(fields):
        if field.col_type == "string":
            if not r_value.match(field.field):
                if 'O' in field.options:
                    binds.append(f"""    if (msg.has_{field.field}())
          bind->set_value_as_{datatype[field.col_type]}({i}, common::truncate_utf8(msg.{field.field}(), get_centreon_storage_{table}_col_size(centreon_storage_{table}_{field.col_name})));
        else
          bind->set_null_{datatype[field.col_type]}({i});"""
    )
                else:
                    binds.append(f'    bind->set_value_as_{datatype[field.col_type]}({i}, common::truncate_utf8(msg.{field.field}(), get_centreon_storage_{table}_col_size(centreon_storage_{table}_{field.col_name})));')
            else:
                binds.append(f'    bind->set_value_as_{datatype[field.col_type]}({i}, common::truncate_utf8({field.field[2:-1]}, get_centreon_storage_{table}_col_size(centreon_storage_{table}_{field.col_name})));')
        else:
            if not r_value.match(field.field):
                if 'O' in field.options:
                    binds.append(f"""    if (msg.has_{field.field}())
          bind->set_value_as_{datatype[field.col_type]}({i}, msg.{field.field}());
        else
          bind->set_null_{datatype[field.col_type]}({i});"""
    )
                else:
                    binds.append(f'    bind->set_value_as_{datatype[field.col_type]}({i}, msg.{field.field}());')
            else:
                binds.append(f'    bind->set_value_as_{datatype[field.col_type]}({i}, {field.field[2:-1]});')
    bind_code = "\n".join(binds)
    return f"""/**
 * @brief Complete the {table} table with the given messages (code for MariaDB).
 *
 * @param lst The list of messages to add/update.
 */
void database_configurator::_add_hosts_mariadb(const ::google::protobuf::RepeatedPtrField<{pb_msg}>& lst) {{
  std::string query("{query_prefix} ({params}) {query_suffix}");
  mysql_bulk_stmt stmt(query);
  mysql& mysql = _stream->get_mysql();
  if (!stmt.prepared())
    mysql.prepare_statement(stmt);

  auto bind = stmt.create_bind();

  for (const auto& msg : lst) {{
{bind_code}
    bind->next_row();
  }}
  stmt.set_bind(std::move(bind));
  mysql.run_statement(stmt);
}}
"""

# Generate the _add_hosts_mysql function
def generate_query_mysql(query, pb_msg, table, fields):
    if query == "INSERT ON DUPLICATE KEY UPDATE":
        query_prefix = f"INSERT INTO {table} VALUES"
        query_suffix = "ON DUPLICATE KEY UPDATE "
        query_suffix += ",".join(
            f"{field.col_name} = VALUES({field.col_name})"
            for field in fields if not 'U' in field.options
        )
    else:
        print(sys.stderr, "Error: Unsupported query type.")
        sys.exit(2)
    values_str = ",".join(
        "'{}'"
        if field.field_type == "string" else "{}"
        for field in fields
    )
    values = []
    for field in fields:
        m_value = r_value.match(field.field)
        if m_value:
            if field.field_type == "string":
                values.append(f"misc::string::escape({m_value.group(1)}, get_centreon_storage_{table}_col_size(centreon_storage_{table}_{field.col_name}))")
            else:
                values.append(f"{m_value.group(1)}")
        else:
            if field.field_type == "string":
                values.append(f"misc::string::escape(msg.{field.field}(), get_centreon_storage_{table}_col_size(centreon_storage_{table}_{field.col_name}))")
            else:
                values.append(f"msg.{field.field}()")
    values_code = ", ".join(values)
    return f"""
/**
 * @brief Complete the {table} table with the given messages (code for MySQL).
 *
 * @param lst The list of messages to add/update.
 */
void database_configurator::_add_hosts_mysql(const ::google::protobuf::RepeatedPtrField<{pb_msg}>& lst) {{
  mysql& mysql = _stream->get_mysql();

  std::vector<std::string> values;
  for (const auto& msg : lst) {{
    std::string value(
        fmt::format("({values_str})", {values_code}));
    values.emplace_back(value);
  }}
  std::string query(fmt::format("{query_prefix} {{}} {query_suffix}", fmt::join(values, ",")));
  mysql.run_query(query);
}}
"""

# Process the file and generate the output
output_lines = []

for line in lines:
    output_lines.append(line)
    if not in_block:
        if r_start.match(line):
            in_block = True
            fields = []
            query = ""
    else:
        m_query = r_query.match(line)
        m_field = r_field.match(line)
        m_pb_msg = r_pb_msg.match(line)
        m_table = r_table.match(line)
        if m_query:
            query = m_query.group(1).strip()
        elif m_table:
            table = m_table.group(1).strip()
        elif m_pb_msg:
            pb_msg = m_pb_msg.group(1).strip()
        elif m_field:
            if not r_options.match(m_field.group(5)):
                print(f"""Error: Invalid options in row:\n{m_field.group(0)}:\n Only 'O' and 'U' are authorized:
  -O: to specify that the field is optional (in the database it can be NULL)
  -U: to specify that the field is a unique key, in an update it is not changed.
  """, file=sys.stderr)
                sys.exit(3)

            fields.append(Field(field = m_field.group(1), field_type = m_field.group(2), col_name = m_field.group(3), col_type = m_field.group(4), options = m_field.group(5)))
        elif line.strip() == "*/":
            in_block = False
            # Generate and insert the functions after the comment block
            output_lines.append(generate_query_mariadb(query, pb_msg, table, fields))
            output_lines.append("\n")
            output_lines.append(generate_query_mysql(query, pb_msg, table, fields))
            output_lines.append("\n")

# Write the output to the new file
with open(file_out, 'w') as f:
    f.writelines(output_lines)

print(f"Generated file written to {file_out}")
