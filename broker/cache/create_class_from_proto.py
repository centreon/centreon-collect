import re
import sys
from pathlib import Path


'''
Module: create_class_from_proto

This module generates C++ header and source files from protobuf definitions.
It parses .proto files, extracts message and enum definitions, and creates
corresponding C++ classes optimized for use in shared memory segments.

The generated classes provide:
- Type-safe wrappers around protobuf objects
- Support for optional, repeated, and required fields
- Update mechanisms to sync with protobuf changes
- Field enumeration for serialization
- Compatibility with interprocess shared memory allocators

'''


def remove_comments(text: str) -> str:
    """
    Remove C++ style comments from text.
    Handles both /* */ block comments and // line comments.

    Args:
        text (str): Text containing comments to remove

    Returns:
        str: Text with all comments removed
    """
    # Remove /* */ comments
    text = re.sub(r'/\*.*?\*/', '', text, flags=re.S)
    # Remove // comments
    text = re.sub(r'//.*', '', text)
    return text


def extract_blocks(text, keyword):
    """
    Extract named blocks (messages or enums) from protobuf text.

    Uses regex to find block declarations like "message Name {}" or "enum Name {}",
    then uses brace counting to extract the complete block content.

    Args:
        text (str): Protobuf file content
        keyword (str): Block type to extract ("message" or "enum")

    Returns:
        list: List of tuples (name, block_content) for each found block
    """
    pattern = re.compile(rf'\b{keyword}\s+(\w+)\s*\{{', re.MULTILINE)
    results = []

    for match in pattern.finditer(text):
        name = match.group(1)
        start = match.end()
        brace_count = 1
        i = start

        while i < len(text) and brace_count > 0:
            if text[i] == '{':
                brace_count += 1
            elif text[i] == '}':
                brace_count -= 1
            i += 1

        block_content = text[start:i - 1].strip()
        results.append((name, block_content))

    return results


def extract_fields(message_body):
    """
    Extract field definitions from a protobuf message body.

    Parses field declarations to extract:
    - Field label (optional, required, repeated)
    - Field type (builtin or custom type)
    - Field name
    - Field index number

    Args:
        message_body (str): Content of a message block

    Returns:
        list: List of dicts with keys: name, type, label, index
    """
    fields = []

    field_pattern = re.compile(
        r'(?:(optional|required|repeated)\s+)?'  # label
        r'([\w\.<>]+)\s+'                        # type
        r'(\w+)\s*=\s*'                          # name
        r'(\d+)',                                # index
        re.MULTILINE
    )

    for match in field_pattern.finditer(message_body):
        label = match.group(1) or "implicit"
        field_type = match.group(2)
        field_name = match.group(3)
        index = int(match.group(4))

        fields.append({
            "name": field_name,
            "type": field_type,
            "label": label,
            "index": index
        })

    return fields


def extract_enum_values(enum_body):
    """
    Extract enum value definitions from a protobuf enum body.

    Args:
        enum_body (str): Content of an enum block

    Returns:
        list: List of tuples (enum_name, numeric_value)
    """
    values = []

    enum_pattern = re.compile(
        r'(\w+)\s*=\s*(\d+)',
        re.MULTILINE
    )

    for match in enum_pattern.finditer(enum_body):
        name = match.group(1)
        number = int(match.group(2))
        values.append((name, number))

    return values


def parse_proto_file(filepath):
    """
    Parse a complete .proto file.

    Reads the file, removes comments, and extracts all message and enum definitions.

    Args:
        filepath (str): Path to the .proto file

    Returns:
        tuple: (enums, messages) where each is a list of (name, content) tuples
    """
    text = Path(filepath).read_text()
    text = remove_comments(text)

    messages = extract_blocks(text, "message")

    enums = extract_blocks(text, "enum")

    return enums, messages


def add_used_class_and_dependencies(class_definitions: dict, class_dependencies: dict, class_name: str):
    """
    Recursively collect a class and all its dependencies.

    Builds a dictionary containing the requested class and all classes it depends on.
    Used to generate only the necessary class definitions when specific classes are requested.

    Args:
        class_definitions (dict): Mapping of class names to their definitions
        class_dependencies (dict): Mapping of class names to their dependency lists
        class_name (str): Name of the class to start from

    Returns:
        dict: Dictionary of class_name -> definition for the class and all dependencies
    """
    ret = {}
    if class_name in class_definitions:
        ret[class_name] = class_definitions[class_name]
    if class_name in class_dependencies:
        for used_class in class_dependencies[class_name]:
            ret.update(add_used_class_and_dependencies(class_definitions,
                       class_dependencies, used_class))
    return ret


def add_class_and_dependencies(class_definitions: dict, class_dependencies: dict, requested_classes: dict):
    """
    Collect multiple classes and all their transitive dependencies.

    Iterates over requested classes and recursively adds their dependencies.

    Args:
        class_definitions (dict): Mapping of class names to definitions
        class_dependencies (dict): Mapping of class names to dependency lists
        requested_classes (dict): Dictionary of requested class names

    Returns:
        dict: Dictionary of all classes needed (requested + dependencies)
    """
    ret = {}
    for class_name in requested_classes:
        ret.update(add_used_class_and_dependencies(class_definitions,
                   class_dependencies, class_name))
    return ret


def create_header(protos: list, messages: dict, enums: dict, classes: list):
    """
    Generate C++ header file from protobuf messages.

    Creates protobuf.hh containing:
    - Apache 2.0 license header
    - Include guards and protobuf includes
    - Base message class with enum of all message types
    - Generated message wrapper classes with:
        - Tuple-based data storage
        - Mutable accessors
        - Const accessors
        - Copy/move deletion
        - Update methods
    - Field enumeration support

    Writes output to: broker/core/inc/com/centreon/broker/cache/protobuf.hh

    Args:
        protos (list): List of proto file basenames
        messages (dict): Dictionary of message_name -> message_body
        enums (dict): Dictionary of enum_name -> enum_body
        classes (list): List of specific classes to generate (empty = all)
    """

    hh = '''/**
 * Copyright 2026 Centreon
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

#ifndef CCB_CACHE_PROTOBUF_HH
#define CCB_CACHE_PROTOBUF_HH

'''

    for proto in protos:
        hh += f'#include "bbdo/{proto}.pb.h"\n'

    class_definitions = {}
    class_dependencies = {}

    for pb_class_name, body in messages.items():
        snake_name = camel_to_snake(pb_class_name)

        data_type_tuple_define = []
        modifiers = []
        accessors = []
        data_initializer = ''
        fields = extract_fields(body)

        if not fields:
            continue

        tuple_index = 0
        for field in fields:
            name = field['name']
            type = field['type']
            label = field['label']
            index = field['index']
            cpp_type = ''
            if label == 'optional':
                if type in ["int32", "uint32", "int64", "uint64"]:
                    cpp_type = f'std::optional<{type}_t>'
                elif type in ["double", "float", "string", "bool"]:
                    cpp_type = f'std::optional<{type}>'
                else:
                    if type in enums:  # convert all enums to int32
                        cpp_type = "std::optional<int32_t>"
                    if type in messages:
                        ccp_type = "message::pointer"
                        if not pb_class_name in class_dependencies:
                            class_dependencies[pb_class_name] = []
                        class_dependencies[pb_class_name].append(type)

            elif label == 'repeated':
                if type in ["int32", "uint32", "int64", "uint64", "double", "float", "string", "bool"]:
                    cpp_type = f"{type}_vect"
                else:
                    if type in enums:  # convert all enums to int32
                        cpp_type = "int32_vect"
                    elif type in messages:
                        cpp_type = "mess_vect"
                        if not pb_class_name in class_dependencies:
                            class_dependencies[pb_class_name] = []
                        class_dependencies[pb_class_name].append(type)
            else:
                if type in ["int32", "uint32", "int64", "uint64"]:
                    cpp_type = f'{type}_t'
                elif type in ["double", "float", "string", "bool"]:
                    cpp_type = type
                else:
                    if type in enums:  # convert all enums to int32
                        cpp_type = "int32_t"
                    if type in messages:
                        cpp_type = "message::pointer"
                        if not pb_class_name in class_dependencies:
                            class_dependencies[pb_class_name] = []
                        class_dependencies[pb_class_name].append(type)

            if cpp_type != '':
                data_type_tuple_define.append(f"{cpp_type} /* {name} */")
                modifiers.append(
                    f"  {cpp_type}& mutable_{name}() {{ return std::get<{tuple_index}>(_data); }}")
                accessors.append(
                    f"  const {cpp_type}& {name}() const {{ return std::get<{tuple_index}>(_data); }}")
                tuple_index += 1

        # build class declaration that may be not used
        class_declaration = f'''/**
 * @brief class {snake_name} constructed from {pb_class_name}
 *
 */
class {snake_name} : public message {{
'''
        class_declaration += "  using data_type = std::tuple<\n"
        class_declaration += "                               "
        class_declaration += ",\n                               ".join(
            data_type_tuple_define)
        class_declaration += """\n                               >;\n
  data_type _data;

  friend bool update(const ::google::protobuf::Message& mess);

"""
        class_declaration += f'''

 public:
  {snake_name}(const {pb_class_name}& src, const allocators& allocator);
  {snake_name}(const {snake_name} &) = delete;
  {snake_name} & operator = (const {snake_name} &) = delete;
  ~{snake_name}();

  bool update(const {pb_class_name}& mess, const allocators& allocator);

  std::vector<variant> enumerate_fields() const;

'''
        class_declaration += "\n".join(modifiers)
        class_declaration += "\n\n"
        class_declaration += "\n".join(accessors)
        class_declaration += '\n};\n\n\n'

        class_definitions[pb_class_name] = class_declaration

    if len(classes) == 0:
        used_class = class_definitions
    else:
        used_class = add_class_and_dependencies(
            class_definitions, class_dependencies, classes)

    hh_class_enum_list = [
        f"e_{camel_to_snake(pb_class_name)}" for pb_class_name in used_class]

    # now we build all header file content
    message_enum_str = ",\n    ".join(hh_class_enum_list)

    hh += f'''
#include "protobuf_utils.hh"

namespace com::centreon::broker::cache {{

/**
 * All following classes are generated by create_class_from_proto.py
 * As protobuf object can't be stored in mapped memory, we create alter ego of
 * protobuf objects.
 *
 */

/**
 * @brief base class for all following objects
 *
 */
class message {{
 public:
  enum class e_type {{
    {message_enum_str}
  }};

 private:
  e_type _type;

 protected:
  char_allocator _char_alloc;

 public:
  using pointer = interprocess::offset_ptr<message>;

  message(e_type typ, const char_allocator char_alloc)
      : _type(typ), _char_alloc(char_alloc) {{}}

  bool update(const ::google::protobuf::Message& mess,
              const allocators& allocator);

  std::vector<variant> enumerate_fields() const;
}};

'''

    for class_to_add in used_class.values():
        hh += class_to_add

    hh += '''}  // namespace com::centreon::broker::cache

#endif
'''
    with open('broker/cache/inc/com/centreon/broker/cache/protobuf.hh', 'w') as hh_file:
        hh_file.write(hh)


def create_cc(messages, enums, classes):
    """
    Generate C++ source file with message implementations.

    Creates protobuf.cc containing:
    - Apache 2.0 license header
    - Helper macros for field enumeration
    - Helper macros for field updates (handles optional, repeated, nested messages)
    - Base message class implementations
    - Generated message wrapper implementations with:
        - Constructor: builds tuple from protobuf object
        - Destructor: cleans up nested message pointers
        - enumerate_fields(): returns vector of field variants
        - update(): syncs fields from protobuf, returns whether changed
    - Virtual dispatch in base class for polymorphic operations

    Writes output to: broker/core/src/cache/protobuf.cc

    Args:
        messages (dict): Dictionary of message_name -> message_body
        enums (dict): Dictionary of enum_name -> enum_body
        classes (list): List of specific classes to generate (empty = all)
    """
    cc = ''
    class_implementations = {}
    class_dependencies = {}

    for pb_class_name, body in messages.items():
        snake_name = camel_to_snake(pb_class_name)
        tuple_init = []
        repeated_fillers = []
        update_fields = []
        repeated_mess_deleter = []
        enumerate_field = []

        fields = extract_fields(body)

        if not fields:
            continue

        tuple_index = 0
        for field in fields:
            name = field['name']
            type = field['type']
            label = field['label']
            index = field['index']

            field_tuple_init = ''

            if label == 'optional':
                if type in ["int32", "uint32", "int64", "uint64"]:
                    field_tuple_init = f"src.has_{name}()?std::optional<{type}_t>(src.{name}()):std::optional<{type}_t>()"
                    update_fields.append(f"UPDATE_OPTIONAL_FIELD({name})")
                    enumerate_field.append(
                        f"ADD_ENUMERATION_FIELD({tuple_index});")
                elif type in ["double", "float", "bool"]:
                    field_tuple_init = f"src.has_{name}()?std::optional<{type}>(src.{name}()):std::optional<{type}>()"
                    update_fields.append(f"UPDATE_OPTIONAL_FIELD({name})")
                    enumerate_field.append(
                        f"ADD_ENUMERATION_FIELD({tuple_index});")
                elif type == "string":
                    field_tuple_init = f"src.has_{name}()?std::optional<{type}>(string(src.{name}().c_str(), src.{name}().length(), allocator.char_alloc)):std::optional<{type}>()"
                    update_fields.append(
                        f"UPDATE_OPTIONAL_STRING_FIELD({name})")
                    enumerate_field.append(
                        f"ADD_ENUMERATION_OPTIONAL_STRING_FIELD({tuple_index});")
                else:
                    if type in enums:  # convert all enums to int32
                        field_tuple_init = f"src.has_{name}()?std::optional<int32_t>(src.{name}()):std::optional<int32_t>()"
                        update_fields.append(
                            f"UPDATE_OPTIONAL_FIELD({name})")
                        enumerate_field.append(
                            f"ADD_ENUMERATION_FIELD({tuple_index});")
                    if type in messages:
                        field_tuple_init = f"src.has_{name}()?allocator.segm_manager->construct<{camel_to_snake(type)}>(interprocess::anonymous_instance)(src.{name}(), allocator):nullptr"
                        update_fields.append(
                            f"UPDATE_OPTIONAL_MESS_FIELD({camel_to_snake(type)}, {name})")
                        enumerate_field.append(
                            f"ADD_ENUMERATION_OPTIONAL_MESS_FIELD({tuple_index});")

            elif label == 'repeated':
                if type in ["int32", "uint32", "int64", "uint64", "double", "float", "bool"]:
                    field_tuple_init = f"allocator.{type}_alloc"
                    repeated_fillers.append(f'''    mutable_{name}().reserve(src.{name}().size());
    for (const auto & value: src.{name}()) {{
        mutable_{name}().push_back(value);
    }}''')
                    update_fields.append(f"UPDATE_REPEATED_FIELD({name})")
                    enumerate_field.append(
                        f"ADD_ENUMERATION_FIELD({tuple_index});")
                elif type == "string":
                    field_tuple_init = f"allocator.string_alloc"
                    repeated_fillers.append(f'''    mutable_{name}().reserve(src.{name}().size());
    for (const auto & value: src.{name}()) {{
        mutable_{name}().emplace_back(value.c_str(), value.length(), allocator.char_alloc);
    }}''')
                    update_fields.append(
                        f"UPDATE_REPEATED_STRING_FIELD({name})")
                    enumerate_field.append(
                        f"ADD_ENUMERATION_FIELD({tuple_index});")
                else:
                    if type in enums:  # convert all enums to int32
                        field_tuple_init = "allocator.int32_alloc"
                        update_fields.append(
                            f"UPDATE_REPEATED_FIELD({name})")
                        enumerate_field.append(
                            f"ADD_ENUMERATION_FIELD({tuple_index});")
                    elif type in messages:
                        field_tuple_init = "allocator.message_alloc"
                        if not pb_class_name in class_dependencies:
                            class_dependencies[pb_class_name] = []
                        class_dependencies[pb_class_name].append(type)
                        repeated_fillers.append(f'''    mutable_{name}().reserve(src.{name}().size());
    for (const auto & mess: src.{name}()) {{
        mutable_{name}().push_back(allocator.segm_manager->construct<{camel_to_snake(type)}>(
        interprocess::anonymous_instance)(mess, allocator));
    }}''')
                        repeated_mess_deleter.append(
                            f"  REPEATED_MESS_DELETE_ALL({camel_to_snake(type)}, {name});")
                        update_fields.append(
                            f"UPDATE_REPEATED_MESS_FIELD({camel_to_snake(type)}, {name})")
                        enumerate_field.append(
                            f"ADD_ENUMERATION_FIELD({tuple_index});")
            else:
                if type in ["int32", "uint32", "int64", "uint64", "double", "float", "bool"]:
                    field_tuple_init = f"src.{name}()"
                    update_fields.append(f"UPDATE_FIELD({name})")
                    enumerate_field.append(
                        f"ADD_ENUMERATION_FIELD({tuple_index});")
                elif type == "string":
                    field_tuple_init = f"string(src.{name}().c_str(), src.{name}().length(), allocator.char_alloc)"
                    update_fields.append(f"UPDATE_STRING_FIELD({name})")
                    enumerate_field.append(
                        f"ADD_ENUMERATION_STRING_FIELD({tuple_index});")
                else:
                    if type in enums:  # convert all enums to int32
                        field_tuple_init = f"src.{name}()"
                        update_fields.append(f"UPDATE_FIELD({name})")
                        enumerate_field.append(
                            f"ADD_ENUMERATION_FIELD({tuple_index});")
                    if type in messages:
                        field_tuple_init = f"allocator.segm_manager->construct<{camel_to_snake(type)}>(interprocess::anonymous_instance)(src.{name}(), allocator)"
                        update_fields.append(
                            f"UPDATE_MESS_FIELD({name})")
                        enumerate_field.append(
                            f"ADD_ENUMERATION_MESS_FIELD({tuple_index});")
                        if not pb_class_name in class_dependencies:
                            class_dependencies[pb_class_name] = []
                        class_dependencies[pb_class_name].append(type)

            if field_tuple_init != '':
                tuple_init.append(field_tuple_init)
                tuple_index += 1

        tuple_init_str = ",\n        ".join(tuple_init)
        repeated_fillers_str = "\n".join(repeated_fillers)
        repeated_mess_deleter_str = "\n".join(repeated_mess_deleter)
        enumerate_field_str = "\n  ".join(enumerate_field)
        update_fields_str = ";\n  ".join(update_fields)

        class_implementations[pb_class_name] = f'''

/**
 * @brief Construct a new {snake_name} object
 *
 * @param src probuf objects
 * @param allocator allocators
 */
{snake_name}::{snake_name}(const {pb_class_name}& src, const allocators& allocator)
    : message(e_type::e_{snake_name}, allocator.char_alloc),
    _data{{
        {tuple_init_str}
    }}
{{
{repeated_fillers_str}
}}

/**
 * @brief Destroy the {snake_name} object
 * 
 */
{snake_name}::~{snake_name}() {{
{repeated_mess_deleter_str}
}}

/**
 * @brief enumerate all fields in the same order as protobuf indexes
 *
 * @return std::vector<variant>
 */
std::vector<variant> {snake_name}::enumerate_fields() const {{
  std::vector<variant> ret;
  ret.reserve(std::tuple_size<data_type>::value);
  {enumerate_field_str}
  return ret;
}}

/**
 * @brief update object with a protobuf object
 * 
 * @param mess    proto object
 * @param allocator 
 * @return true at least one field had been modified
 * @return false 
 */
bool {snake_name}::update(const {pb_class_name}& mess, const allocators& allocator) {{
  bool updated = false;
  {update_fields_str}
  return updated;
}}


'''

    if len(classes) == 0:
        used_class = class_implementations
    else:
        used_class = add_class_and_dependencies(
            class_implementations, class_dependencies, classes)

    enumerate_field_impl = []
    update_field_impl = []
    for pb_class_name in used_class:
        snake_name = camel_to_snake(pb_class_name)
        enumerate_field_impl.append(f'''    case e_type::e_{snake_name}:
      return static_cast<const {snake_name}&>(*this).enumerate_fields();''')
        update_field_impl.append(f'''    case e_type::e_{snake_name}:
      return static_cast<{snake_name}&>(*this).update(
          static_cast<const {pb_class_name}&>(mess), allocator);''')

    enumerate_field_impl_str = "\n".join(enumerate_field_impl)
    update_field_impl_str = "\n".join(update_field_impl)

    cc = f'''/**
 * Copyright 2026 Centreon
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

#include "com/centreon/broker/cache/protobuf.hh"

using namespace com::centreon::broker::cache;

/**
 * @brief macros used by enumerate_fields
 *
 */
#define ADD_ENUMERATION_FIELD(field_index) \\
  ret.emplace_back(std::get<field_index>(_data));

#define ADD_ENUMERATION_STRING_FIELD(field_index) \\
  ret.emplace_back(&std::get<field_index>(_data));

#define ADD_ENUMERATION_MESS_FIELD(field_index) \\
  ret.emplace_back(std::get<field_index>(_data).get());

#define ADD_ENUMERATION_OPTIONAL_STRING_FIELD(field_index) \\
  ret.emplace_back(std::get<field_index>(_data)            \\
                       ? &*std::get<field_index>(_data)    \\
                       : nullptr);

#define ADD_ENUMERATION_OPTIONAL_MESS_FIELD(field_index) \\
  ret.emplace_back(std::get<field_index>(_data)          \\
                       ? &*std::get<field_index>(_data)  \\
                       : nullptr);

/**
 * @brief following macros are used to update mapped object from protobuf ones
 *
 */
#define UPDATE_FIELD(field)                \\
  if (mutable_##field() != mess.field()) {{ \\
    mutable_##field() = mess.field();      \\
    updated = true;                        \\
  }}

#define UPDATE_STRING_FIELD(field)                                         \\
  if (mess.field().compare(mutable_##field().c_str())) {{                   \\
    mutable_##field().resize(mess.field().length());                       \\
    mutable_##field().assign(mess.field().c_str(), mess.field().length()); \\
    updated = true;                                                        \\
  }}

#define UPDATE_MESS_FIELD(field)                            \\
  if (mutable_##field()->update(mess.field(), allocator)) {{ \\
    updated = true;                                         \\
  }}

#define UPDATE_OPTIONAL_FIELD(field)                                   \\
  if (mess.has_##field() && !mutable_##field()) {{                     \\
    mutable_##field() = mess.field();                                  \\
    updated = true;                                                    \\
  }} else if (!mess.has_##field() && mutable_##field()) {{             \\
    mutable_##field().reset();                                         \\
    updated = true;                                                    \\
  }} else if (mutable_##field() && mutable_##field() != mess.field()) {{ \\
    mutable_##field() = mess.field();                                  \\
    updated = true;                                                    \\
  }}

#define UPDATE_OPTIONAL_STRING_FIELD(field)                                 \\
  if (mess.has_##field() && !mutable_##field()) {{                           \\
    mutable_##field() =                                                     \\
        string(mess.field().c_str(), mess.field().length(), _char_alloc);   \\
    updated = true;                                                         \\
  }} else if (!mess.has_##field() && mutable_##field()) {{                    \\
    mutable_##field().reset();                                              \\
    updated = true;                                                         \\
  }} else if (mutable_##field() &&                                           \\
             mess.field().compare(mutable_##field()->c_str())) {{            \\
    mutable_##field()->resize(mess.field().length());                       \\
    mutable_##field()->assign(mess.field().c_str(), mess.field().length()); \\
    updated = true;                                                         \\
  }}

#define UPDATE_OPTIONAL_MESS_FIELD(mess_type, field)                       \\
  if (mess.has_##field() && !mutable_##field()) {{                          \\
    mutable_##field() =   allocator.segm_manager->construct<mess_type>(    \\
              interprocess::anonymous_instance)(mess.field(), allocator)); \\
    updated = true;                                                        \\
  }} else if (!mess.has_##field() && mutable_##field()) {{                   \\
    mutable_##field().reset();                                             \\
    updated = true;                                                        \\
  }} else if (mutable_##field()->update(mess.field, allocator)) {{           \\
    updated = true;                                                        \\
  }}

#define UPDATE_REPEATED_FIELD(field)                                           \\
  auto src_iter = mess.field().begin();                                        \\
  auto src_end = mess.field().end();                                           \\
  if (static_cast<unsigned>(mess.field().size()) > mutable_##field().size()) {{ \\
    mutable_##field().reserve(mess.field().size());                            \\
  }}                                                                            \\
  auto dst_iter = mutable_##field().begin();                                   \\
  auto dst_end = mutable_##field().end();                                      \\
  for (; src_iter != src_end && dst_iter != dst_end; ++src_iter, ++dst_iter) {{ \\
    if (*src_iter != *dst_iter) {{                                              \\
      *dst_iter = *src_iter;                                                   \\
      updated = true;                                                          \\
    }}                                                                          \\
  }}                                                                            \\
  if (dst_iter != dst_end) {{                                                   \\
    mutable_##field().erase(dst_iter, dst_end);                                \\
  }}                                                                            \\
  else if (src_iter != src_end) {{                                               \\
    updated = true;                                                            \\
    for (; src_iter != src_end; ++src_iter) {{                                 \\
      mutable_##field().push_back(*src_iter);                                  \\
    }}                                                                          \\
  }}

#define UPDATE_REPEATED_STRING_FIELD(field)                                    \\
  auto src_iter = mess.field().begin();                                        \\
  auto src_end = mess.field().end();                                           \\
  if (static_cast<unsigned>(mess.field().size()) > mutable_##field().size()) {{ \\
    mutable_##field().reserve(mess.field().size());                            \\
  }}                                                                            \\
  auto dst_iter = mutable_##field().begin();                                   \\
  auto dst_end = mutable_##field().end();                                      \\
  for (; src_iter != src_end && dst_iter != dst_end; ++src_iter, ++dst_iter) {{ \\
    if (src_iter->compare(dst_iter->c_str())) {{                                \\
      dst_iter->resize(src_iter->length());                                    \\
      dst_iter->assign(src_iter->c_str(), src_iter->length());                 \\
      updated = true;                                                          \\
    }}                                                                          \\
  }}                                                                            \\
  if (dst_iter != dst_end) {{                                                   \\
    mutable_##field().erase(dst_iter, dst_end);                                \\
  }}                                                                            \\
  else if (src_iter != src_end) {{                                              \\
    updated = true;                                                            \\
    for (; src_iter != src_end; ++src_iter) {{                                 \\
      mutable_##field().emplace_back(src_iter->c_str(), src_iter->length(),    \\
                                     allocator.char_alloc);                    \\
    }}                                                                          \\
  }}                                                                            \\

#define UPDATE_REPEATED_MESS_FIELD(mess_type, field)                           \\
  auto src_iter = mess.field().begin();                                        \\
  auto src_end = mess.field().end();                                           \\
  if (static_cast<unsigned>(mess.field().size()) > mutable_##field().size()) {{ \\
    mutable_##field().reserve(mess.field().size());                            \\
  }}                                                                            \\
  auto dst_iter = mutable_##field().begin();                                   \\
  auto dst_end = mutable_##field().end();                                      \\
  for (; src_iter != src_end && dst_iter != dst_end; ++src_iter, ++dst_iter) {{ \\
    if ((*dst_iter)->update(*src_iter, allocator)) {{                           \\
      updated = true;                                                          \\
    }}                                                                          \\
  }}                                                                            \\
  if (dst_iter != dst_end) {{                                                   \\
    for (auto to_destroy = dst_iter; to_destroy != dst_end; ++to_destroy) {{    \\
      allocator.segm_manager->destroy_ptr(                                     \\
          static_cast<mess_type*>(to_destroy->get()));                         \\
    }}                                                                          \\
    mutable_##field().erase(dst_iter, dst_end);                                \\
  }}                                                                            \\
  else if (src_iter != src_end) {{                                              \\
    updated = true;                                                            \\
    for (; src_iter != src_end; ++src_iter) {{                                 \\
      mutable_##field().push_back(                                             \\
          allocator.segm_manager->construct<mess_type>(                        \\
              interprocess::anonymous_instance)(*src_iter, allocator));        \\
    }}                                                                          \\
  }}                                                                            \\

#define REPEATED_MESS_DELETE_ALL(mess_type, field)  \\
  for (auto to_delete : mutable_##field()) {{        \\
    _char_alloc.get_segment_manager()->destroy_ptr( \\
        static_cast<mess_type*>(to_delete.get()));  \\
  }}

/**
 * @brief enumerate all fields of object
 * As virtual methods are not allowed in mapped segment, we cast message to its
 * real type
 *
 * @return std::vector<variant> list of fields
 */
std::vector<variant> message::enumerate_fields() const {{
  switch (_type) {{
{enumerate_field_impl_str}
    default:
      return {{}};
  }}
}}

/**
 * @brief update a protobuf message from protobuf
 * In order to optimize disk usage, every field is first compared and then
 * updated
 * @param mess proto
 * @return true updated
 * @return false not updated
 */
bool message::update(const ::google::protobuf::Message& mess,
                     const allocators& allocator) {{
  switch (_type) {{
  {update_field_impl_str}
    default:
      return false;
  }}
}}


'''

    for class_impl in used_class.values():
        cc += class_impl

    with open('broker/cache/src/protobuf.cc', 'w') as cc_file:
        cc_file.write(cc)


def camel_to_snake(name: str) -> str:
    """
    Convert CamelCase identifier to snake_case.

    Handles transitions from lowercase to uppercase letters and
    digit/character to uppercase letter transitions.
    Example: HTTPServerCache -> http_server_cache

    Args:
        name (str): CamelCase name to convert

    Returns:
        str: snake_case version of the name
    """
    s1 = re.sub('(.)([A-Z][a-z]+)', r'\1_\2', name)
    s2 = re.sub('([a-z0-9])([A-Z])', r'\1_\2', s1)
    return s2.lower()


if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: from root project folder python create_class_from_proto.py <protos basename without extension separated by a comma (no space) ex: neb,storage> <list of protobuf class separated by a comma (no space) ex: Host,Service")
        sys.exit(1)

    protos = sys.argv[1].split(',')
    classes = []
    if len(sys.argv) >= 3:
        classes = {value: i for i, value in enumerate(sys.argv[2].split(','))}

    enums = {}
    messages = {}

    for proto in protos:

        file_enums, file_messages = parse_proto_file(f"bbdo/{proto}.proto")
        enums.update(file_enums)
        messages.update(file_messages)

    create_header(protos, messages, enums, classes)
    create_cc(messages, enums, classes)
