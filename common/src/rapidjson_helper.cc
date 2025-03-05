/**
 * Copyright 2022-2024 Centreon
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

#include <rapidjson/error/en.h>
#include <rapidjson/filereadstream.h>
#include <rapidjson/stringbuffer.h>

#include "rapidjson_helper.hh"

using namespace com::centreon::common;

namespace com::centreon::common::detail {

class string_view_stream {
  const std::string_view _src;
  std::string_view::const_iterator _current;

 public:
  using Ch = char;

  string_view_stream(const std::string_view src) : _src(src) {
    _current = _src.begin();
  }

  inline Ch Peek() const { return _current >= _src.end() ? '\0' : *_current; }
  inline Ch Take() { return _current >= _src.end() ? '\0' : *_current++; }
  inline size_t Tell() const {
    return static_cast<size_t>(_current - _src.begin());
  }

  Ch* PutBegin() {
    RAPIDJSON_ASSERT(false);
    return 0;
  }
  void Put(Ch) { RAPIDJSON_ASSERT(false); }
  void Flush() { RAPIDJSON_ASSERT(false); }
  size_t PutEnd(Ch*) {
    RAPIDJSON_ASSERT(false);
    return 0;
  }
};

};  // namespace com::centreon::common::detail

/**
 * @brief allow to use for (const auto & member: val)
 *
 * @return rapidjson::Value::ConstValueIterator
 * @throw msg_fmt if oject is not an array
 */
rapidjson::Value::ConstValueIterator rapidjson_helper::begin() const {
  if (!_val.IsArray()) {
    throw exceptions::msg_fmt("object is not an array:{}", _val);
  }
  return _val.Begin();
}

/**
 * @brief read a string field
 *
 * @param field_name
 * @return const char* field value
 * @throw msg_fmt if member does not exist or field value is not a string
 */
const char* rapidjson_helper::get_string(const char* field_name) const {
  return get<const char*>(field_name, "string", &rapidjson::Value::IsString,
                          &rapidjson::Value::GetString);
}

const char* rapidjson_helper::get_string(const char* field_name,
                                         const char* default_value) const {
  return get_or_default<const char*>(
      field_name, "string", &rapidjson::Value::IsString,
      &rapidjson::Value::GetString, default_value);
}

/**
 * @brief read a double field
 *
 * @param field_name
 * @return const char* field value
 * @throw msg_fmt if member does not exist or field value is nor a double nor a
 * string containing a double
 */
double rapidjson_helper::get_double(const char* field_name) const {
  return get<double>(
      field_name, "double",
      [](const rapidjson::Value& val) {
        return val.IsDouble() || val.IsInt() || val.IsUint() || val.IsInt64() ||
               val.IsUint64();
      },
      &rapidjson::Value::GetDouble, &absl::SimpleAtod);
}

/**
 * @brief read a float field
 *
 * @param field_name
 * @return const char* field value
 * @throw msg_fmt if member does not exist or field value is nor a float nor a
 * string containing a float
 */
float rapidjson_helper::get_float(const char* field_name) const {
  return get<float>(
      field_name, "float",
      [](const rapidjson::Value& val) {
        return val.IsFloat() || val.IsInt() || val.IsUint() || val.IsInt64() ||
               val.IsUint64();
      },
      &rapidjson::Value::GetFloat, &absl::SimpleAtof);
}

/**
 * @brief read a uint64_t field
 *
 * @param field_name
 * @return const char* field value
 * @throw msg_fmt if member does not exist or field value is nor a uint64_t nor
 * a string containing a uint64_t
 */
uint64_t rapidjson_helper::get_uint64_t(const char* field_name) const {
  return get<uint64_t>(
      field_name, "uint64",
      [](const rapidjson::Value& val) { return val.IsUint64(); },
      &rapidjson::Value::GetUint64, &absl::SimpleAtoi<uint64_t>);
}

/**
 * @brief read an uint64_t field
 *
 * @param field_name
 * @param default_value value returned if member does not exist
 * @return const char* field value
 * @throw msg_fmt if field value is nor a integer nor a
 * string containing a integer
 */
uint64_t rapidjson_helper::get_uint64_t(const char* field_name,
                                        uint64_t default_value) const {
  return get_or_default<uint64_t>(
      field_name, "uint64_t",
      [](const rapidjson::Value& val) { return val.IsUint64(); },
      &rapidjson::Value::GetUint64, &absl::SimpleAtoi<uint64_t>, default_value);
}

/**
 * @brief read a int64_t field
 *
 * @param field_name
 * @return const char* field value
 * @throw msg_fmt if member does not exist or field value is nor a int64_t nor
 * a string containing a int64_t
 */
int64_t rapidjson_helper::get_int64_t(const char* field_name) const {
  return get<int64_t>(
      field_name, "int64",
      [](const rapidjson::Value& val) { return val.IsInt64(); },
      &rapidjson::Value::GetInt64, &absl::SimpleAtoi<int64_t>);
}

/**
 * @brief read a uint32_t field
 *
 * @param field_name
 * @return const char* field value
 * @throw msg_fmt if member does not exist or field value is nor a uint32_t nor
 * a string containing a uint32_t
 */
uint32_t rapidjson_helper::get_uint32_t(const char* field_name) const {
  uint64_t to_test = get_uint64_t(field_name);
  if (to_test > std::numeric_limits<uint32_t>::max()) {
    throw exceptions::msg_fmt("field {}:uint32_t overflow {}", field_name,
                              to_test);
  }
  return to_test;
}

/**
 * @brief read a int32_t field
 *
 * @param field_name
 * @return const char* field value
 * @throw msg_fmt if member does not exist or field value is nor a int32_t nor
 * a string containing a int32_t
 */
int32_t rapidjson_helper::get_int32_t(const char* field_name) const {
  int64_t to_test = get_int64_t(field_name);
  if (to_test > std::numeric_limits<int32_t>::max() ||
      to_test < std::numeric_limits<int32_t>::min()) {
    throw exceptions::msg_fmt("field {}:int32_t overflow {}", field_name,
                              to_test);
  }
  return to_test;
}

/**
 * @brief read a uint16_t field
 *
 * @param field_name
 * @return const char* field value
 * @throw msg_fmt if member does not exist or field value is nor a uint16_t nor
 * a string containing a uint16_t
 */
uint16_t rapidjson_helper::get_uint16_t(const char* field_name) const {
  uint64_t to_test = get_uint64_t(field_name);
  if (to_test > std::numeric_limits<uint16_t>::max()) {
    throw exceptions::msg_fmt("field {}:uint16_t overflow {}", field_name,
                              to_test);
  }
  return to_test;
}

/**
 * @brief read a int16_t field
 *
 * @param field_name
 * @return const char* field value
 * @throw msg_fmt if member does not exist or field value is nor a int16_t nor
 * a string containing a int16_t
 */
int16_t rapidjson_helper::get_int16_t(const char* field_name) const {
  int64_t to_test = get_int64_t(field_name);
  if (to_test > std::numeric_limits<int16_t>::max() ||
      to_test < std::numeric_limits<int16_t>::min()) {
    throw exceptions::msg_fmt("field {}:int16_t overflow {}", field_name,
                              to_test);
  }
  return to_test;
}

/**
 * @brief read an unsigned integer field
 *
 * @param field_name
 * @param default_value value returned if member does not exist
 * @return const char* field value
 * @throw msg_fmt if field value is nor a integer nor a
 * string containing a integer
 */
unsigned rapidjson_helper::get_unsigned(const char* field_name,
                                        unsigned default_value) const {
  return get_or_default<unsigned>(
      field_name, "unsigned int",
      [](const rapidjson::Value& val) { return val.IsUint(); },
      &rapidjson::Value::GetUint, &absl::SimpleAtoi<unsigned>, default_value);
}

/**
 * @brief read an unsigned int field
 *
 * @param field_name
 * @return const char* field value
 * @throw msg_fmt if member does not exist or field value is nor a uint nor a
 * string containing a uint
 */
unsigned rapidjson_helper::get_unsigned(const char* field_name) const {
  return get<unsigned>(
      field_name, "unsigned int",
      [](const rapidjson::Value& val) { return val.IsUint(); },
      &rapidjson::Value::GetUint, &absl::SimpleAtoi<unsigned>);
}

/**
 * @brief read a integer field
 *
 * @param field_name
 * @param default_value value returned if member does not exist
 * @return const char* field value
 * @throw msg_fmt if field value is nor a integer nor a
 * string containing a integer
 */
int rapidjson_helper::get_int(const char* field_name, int default_value) const {
  return get_or_default<int>(
      field_name, "integer",
      [](const rapidjson::Value& val) { return val.IsInt(); },
      &rapidjson::Value::GetInt, &absl::SimpleAtoi<int>, default_value);
}

/**
 * @brief read a integer field
 *
 * @param field_name
 * @return const char* field value
 * @throw msg_fmt if member does not exist or field value is nor a integer nor a
 * string containing a integer
 */
int rapidjson_helper::get_int(const char* field_name) const {
  return get<int>(
      field_name, "integer",
      [](const rapidjson::Value& val) { return val.IsInt(); },
      &rapidjson::Value::GetInt, &absl::SimpleAtoi<int>);
}

/**
 * @brief read a boolean field
 *
 * @param field_name
 * @return const char* field value
 * @throw msg_fmt if member does not exist or field value is nor a boolean nor a
 * string containing a boolean
 */
bool rapidjson_helper::get_bool(const char* field_name) const {
  return get<bool>(
      field_name, "boolean",
      [](const rapidjson::Value& val) { return val.IsBool(); },
      &rapidjson::Value::GetBool, &absl::SimpleAtob);
}

/**
 * @brief read a boolean field
 *
 * @param field_name
 * @param default_value value returned if member does not exist
 * @return const char* field value
 * @throw msg_fmt if field value is nor a boolean nor a
 * string containing a boolean
 */
bool rapidjson_helper::get_bool(const char* field_name,
                                bool default_value) const {
  return get_or_default<bool>(
      field_name, "boolean",
      [](const rapidjson::Value& val) { return val.IsBool(); },
      &rapidjson::Value::GetBool, &absl::SimpleAtob, default_value);
}

/**
 * @brief return a member
 *
 * @param field_name
 * @return const rapidjson::Value& member
 * @throw msg_fmt if member does not exist
 */
const rapidjson::Value& rapidjson_helper::get_member(
    const char* field_name) const {
  auto member = _val.FindMember(field_name);
  if (member == _val.MemberEnd()) {
    throw exceptions::msg_fmt("no field {}", field_name);
  }
  return member->value;
}

/**
 * @brief load and parse a json from a file
 *
 * @param path
 * @return rapidjson::Document
 */
rapidjson::Document rapidjson_helper::read_from_file(
    const std::string_view& path) {
  FILE* to_close = fopen(path.data(), "r");
  if (!to_close) {
    throw exceptions::msg_fmt("Fail to read file '{}' : {}", path,
                              strerror(errno));
  }
  std::unique_ptr<FILE, int (*)(FILE*)> f(to_close, fclose);

  char read_buffer[0x10000];
  rapidjson::FileReadStream input_stream(f.get(), read_buffer,
                                         sizeof(read_buffer));

  rapidjson::Document json_doc;
  json_doc.ParseStream(input_stream);

  if (json_doc.HasParseError()) {
    throw exceptions::msg_fmt(
        "Error:  File '{}' should be a json "
        "file: {} at offset {}",
        path, rapidjson::GetParseError_En(json_doc.GetParseError()),
        json_doc.GetErrorOffset());
  }
  return json_doc;
}

/**
 * @brief parse json given in json_content
 *
 * @param json_content
 * @return rapidjson::Document
 */
rapidjson::Document rapidjson_helper::read_from_string(
    const std::string_view& json_content) {
  detail::string_view_stream s(json_content);

  rapidjson::Document json_doc;
  json_doc.ParseStream(s);

  if (json_doc.HasParseError()) {
    throw exceptions::msg_fmt(
        "Error: json is not correct: {} at offset {}",
        rapidjson::GetParseError_En(json_doc.GetParseError()),
        json_doc.GetErrorOffset());
  }
  return json_doc;
}

/**
 * @brief check if document owned by this class is conform to validator
 * Example:
 * @code {.c++}
 * json_validator valid("R(
 * {
 *   "$schema": "http://json-schema.org/draft-07/schema#",
 *   "type": "object"
 *  }
 * )");
 *
 * rapidjson::Document to_validate;
 * rapidjson_helper doc(to_validate);
 * try {
 *    doc.validate(valid);
 * } catch (const std::exception &e) {
 *    SPDLOG_LOGGER_ERROR(logger, "bad document: {}", e.what());
 * }
 *
 * @endcode
 *
 * @param validator validator that contains json schema
 * @throw msg_fmt if document is not validated
 */
void rapidjson_helper::validate(json_validator& validator) {
  rapidjson::SchemaValidator valid(validator._schema);
  if (!_val.Accept(valid)) {
    rapidjson::StringBuffer sb;
    valid.GetInvalidSchemaPointer().StringifyUriFragment(sb);
    std::string err;
    err = fmt::format("document doesn't respect this schema: {}\n",
                      validator.get_json_schema());
    if (sb.GetLength() > 1) {
      err += "Invalid value for ";
      err += sb.GetString();
    }
    err += "\nProblem for schema keyword:";
    err += valid.GetInvalidSchemaKeyword();

    sb.Clear();
    valid.GetInvalidDocumentPointer().StringifyUriFragment(sb);
    if (sb.GetLength() > 1) {
      err += fmt::format("\nInvalid document: {}", sb.GetString());
    }
    throw std::invalid_argument(err);
  }
}

/**
 * @brief Construct a new json validator::json validator object
 *
 * @param json_schema
 */
json_validator::json_validator(const std::string_view& json_schema)
    : _schema(rapidjson_helper::read_from_string(json_schema)),
      _json_schema(json_schema) {}
