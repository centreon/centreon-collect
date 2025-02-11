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

#ifndef CCE_RAPIDJSON_HELPER_HH
#define CCE_RAPIDJSON_HELPER_HH

#include <rapidjson/document.h>
#include <rapidjson/schema.h>
#include <rapidjson/writer.h>

#include "com/centreon/exceptions/msg_fmt.hh"

namespace com::centreon::common {

class json_validator;

/**
 * @brief this class is a helper to rapidjson to get a similar API to
 * nlohmann_json library
 * Another drawback, rapidjson doesn't throw but assert
 * so we test and throw before calling accessors
 *
 * usage:
 * @code {.c++}
 * rapidjson::Document doc;
 * rapidjson_helper json(doc);
 * for (const auto doc_member: json) {
 *   rapidjson_helper json_member(doc_member.value);
 *   try {
 *     std::string_view toto = json_member.get_string("titi");
 *   }
 *   catch (const std::exception &e) {
 *       SPDLOG_LOGGER_ERROR(spdlg, "titi not found in {} : {}",
 *          doc_member.value, e.what());
 *   }
 * }
 *
 * @endcode
 *
 *  The constructor is not expensive, object only get a reference to constructor
 * parameter So this object can be used on the fly:
 * @code {.c++}
 * rapidjson::Document doc;
 * rapidjson_helper(doc).get_string("ererzr");
 * @endcode
 *
 *
 * Beware, the life time of the json value passed to the constructor must exceed
 * the lifetime of the rapidjson_helper
 *
 */
class rapidjson_helper {
  const rapidjson::Value& _val;

 public:
  /**
   * @brief Construct a new rapidjson helper object
   *
   * @param val
   */
  rapidjson_helper(const rapidjson::Value& val) : _val(val) {}

  /**
   * @brief deleted in order to forbid rvalue
   *
   * @param val
   */
  rapidjson_helper(rapidjson::Value&& val) = delete;

  /**
   * @brief allow to use for (const auto & member: val)
   *
   * @return rapidjson::Value::ConstValueIterator
   */
  rapidjson::Value::ConstValueIterator begin() const;

  rapidjson::Value::ConstValueIterator end() const { return _val.End(); }

  /**
   * @brief this method simplify access to rapid json value
   * if field is not found or has not the correct type, it throws an exception
   * this method is used by the following getters
   *
   * @tparam return_type  const char *, double, uint64_t .....
   * @param field_name member field name
   * @param type_name type that will be described in exception message
   * @param original_type_tester Value method like IsString
   * @param original_getter  Value getter like GetString
   * @return * template <typename return_type>  value returned by original
   * getter
   */
  template <typename return_type>
  return_type get(const char* field_name,
                  const char* type_name,
                  bool (rapidjson::Value::*original_type_tester)() const,
                  return_type (rapidjson::Value::*original_getter)()
                      const) const {
    if (!_val.IsObject()) {
      throw exceptions::msg_fmt("not an object parent of field {}", field_name);
    }
    auto member = _val.FindMember(field_name);
    if (member == _val.MemberEnd()) {
      throw exceptions::msg_fmt("no field {}", field_name);
    }
    if (!(member->value.*original_type_tester)()) {
      throw exceptions::msg_fmt("field {} is not a {}", field_name, type_name);
    }

    return (member->value.*original_getter)();
  }

  /**
   * @brief this method simplify access to rapid json value
   * if field is not found or has not the correct type, it throws an exception
   * this method is used by the following getters
   * It also allows number contained in string "456"
   * The conversion param for string is passed in last param
   *
   * @tparam return_type double, uint64_t
   * @param field_name member field name
   * @param type_name type that will be described in exception message
   * @param original_type_tester Value method like IsString
   * @param original_getter  Value getter like GetString
   * @param simple_ato  absl::SimpleAtoi Atod.....
   * @return * template <typename return_type>  value returned by original
   * getter
   */
  template <typename return_type, typename type_tester>
  return_type get(const char* field_name,
                  const char* type_name,
                  const type_tester& tester,
                  return_type (rapidjson::Value::*original_getter)() const,
                  bool (*simple_ato)(absl::string_view, return_type*)) const {
    if (!_val.IsObject()) {
      throw exceptions::msg_fmt("not an object parent of field {}", field_name);
    }
    auto member = _val.FindMember(field_name);
    if (member == _val.MemberEnd()) {
      throw exceptions::msg_fmt("no field {}", field_name);
    }
    if (tester(member->value)) {
      return (member->value.*original_getter)();
    }
    if (member->value.IsString()) {
      return_type ret;
      if (!simple_ato(member->value.GetString(), &ret)) {
        throw exceptions::msg_fmt("field {} is not a {} string", field_name,
                                  type_name);
      }
      return ret;
    } else {
      throw exceptions::msg_fmt("field {} is not a {}", field_name, type_name);
    }
  }

  /**
   * @brief this method simplify access to rapid json value
   * if field is not found it returns default
   * if has not the correct type, it throws an exception
   * this method is used by the following getters
   * It also allows number contained in string "456"
   * The conversion param for string is passed in last param
   *
   * @tparam return_type double, uint64_t
   * @param field_name member field name
   * @param type_name type that will be described in exception message
   * @param original_type_tester Value method like IsString
   * @param original_getter  Value getter like GetString
   * @param default_value value returned if field is missing
   * @return * template <typename return_type>  value returned by original
   * getter
   */
  template <typename return_type, typename type_tester>
  return_type get_or_default(const char* field_name,
                             const char* type_name,
                             const type_tester& original_type_tester,
                             return_type (rapidjson::Value::*original_getter)()
                                 const,
                             const return_type& default_value) const {
    if (!_val.IsObject()) {
      throw exceptions::msg_fmt("not an object parent of field {}", field_name);
    }
    auto member = _val.FindMember(field_name);
    if (member == _val.MemberEnd()) {
      return default_value;
    }
    if (!(member->value.*original_type_tester)()) {
      throw exceptions::msg_fmt("field {} is not a {}", field_name, type_name);
    }

    return (member->value.*original_getter)();
  }

  /**
   * @brief this method simplify access to rapid json value
   * if field is not found it returns default
   * if has not the correct type, it throws an exception
   * this method is used by the following getters
   * It also allows number contained in string "456"
   * The conversion param for string is passed in last param
   *
   * @tparam return_type double, uint64_t
   * @param field_name member field name
   * @param type_name type that will be described in exception message
   * @param original_type_tester Value method like IsString
   * @param original_getter  Value getter like GetString
   * @param simple_ato  absl::SimpleAtoi Atod.....
   * @param default_value value returned if field is missing
   * @return * template <typename return_type>  value returned by original
   * getter
   */
  template <typename return_type, typename type_tester>
  return_type get_or_default(const char* field_name,
                             const char* type_name,
                             const type_tester& tester,
                             return_type (rapidjson::Value::*original_getter)()
                                 const,
                             bool (*simple_ato)(absl::string_view,
                                                return_type*),
                             const return_type& default_value) const {
    if (!_val.IsObject()) {
      throw exceptions::msg_fmt("not an object parent of field {}", field_name);
    }
    auto member = _val.FindMember(field_name);
    if (member == _val.MemberEnd()) {
      return default_value;
    }
    if (tester(member->value)) {
      return (member->value.*original_getter)();
    }
    if (member->value.IsString()) {
      if (!*member->value.GetString()) {
        return default_value;
      }
      return_type ret;
      if (!simple_ato(member->value.GetString(), &ret)) {
        throw exceptions::msg_fmt("field {} is not a {} string", field_name,
                                  type_name);
      }
      return ret;
    } else {
      throw exceptions::msg_fmt("field {} is not a {}", field_name, type_name);
    }
  }

  const char* get_string(const char* field_name) const;
  const char* get_string(const char* field_name,
                         const char* default_value) const;

  double get_double(const char* field_name) const;
  float get_float(const char* field_name) const;

  uint64_t get_uint64_t(const char* field_name) const;
  uint64_t get_uint64_t(const char* field_name, uint64_t default_value) const;
  int64_t get_int64_t(const char* field_name) const;

  uint32_t get_uint32_t(const char* field_name) const;
  int32_t get_int32_t(const char* field_name) const;

  uint16_t get_uint16_t(const char* field_name) const;
  int16_t get_int16_t(const char* field_name) const;

  unsigned get_unsigned(const char* field_name) const;
  unsigned get_unsigned(const char* field_name, unsigned default_value) const;

  int get_int(const char* field_name) const;

  int get_int(const char* field_name, int default_value) const;

  bool get_bool(const char* field_name) const;

  bool get_bool(const char* field_name, bool default_value) const;

  // as overriding can't be done with returned type, we use a templated method
  template <typename value_type>
  value_type get(const char* field_name);

  const rapidjson::Value& get_member(const char* field_name) const;

  /**
   * @brief no assert if _val is not an object
   *
   * @param field_name
   * @return true has the member
   * @return false is not an object or has not this field
   */
  bool has_member(const char* field_name) const {
    return _val.IsObject() && _val.HasMember(field_name);
  }

  void validate(json_validator& validator);

  static rapidjson::Document read_from_file(const std::string_view& path);

  static rapidjson::Document read_from_string(
      const std::string_view& json_content);
};

/**
 * @brief by default doesn't compile
 * Only following specializations are valid
 *
 * @tparam value_type std::string, uint64_t, double....
 * @param field_name
 * @return value_type
 */
template <typename value_type>
inline value_type rapidjson_helper::get(const char* field_name
                                        [[maybe_unused]]) {
  class not_convertible {};
  static_assert(std::is_convertible<value_type, not_convertible>::value);
}

template <>
inline std::string rapidjson_helper::get<std::string>(const char* field_name) {
  return get_string(field_name);
}

template <>
inline double rapidjson_helper::get<double>(const char* field_name) {
  return get_double(field_name);
}

template <>
inline float rapidjson_helper::get<float>(const char* field_name) {
  return get_float(field_name);
}

template <>
inline uint64_t rapidjson_helper::get<uint64_t>(const char* field_name) {
  return get_uint64_t(field_name);
}

template <>
inline int64_t rapidjson_helper::get<int64_t>(const char* field_name) {
  return get_int64_t(field_name);
}

template <>
inline uint32_t rapidjson_helper::get<uint32_t>(const char* field_name) {
  return get_uint32_t(field_name);
}

template <>
inline int32_t rapidjson_helper::get<int32_t>(const char* field_name) {
  return get_int32_t(field_name);
}

template <>
inline uint16_t rapidjson_helper::get<uint16_t>(const char* field_name) {
  return get_uint16_t(field_name);
}

template <>
inline int16_t rapidjson_helper::get<int16_t>(const char* field_name) {
  return get_int16_t(field_name);
}

template <>
inline bool rapidjson_helper::get<bool>(const char* field_name) {
  return get_bool(field_name);
}

/**
 * @brief This class is helper to build json validator from a json string
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
 * std::lock_guard l(valid);
 * try {
 *    doc.validate(valid);
 * } catch (const std::exception &e) {
 *    SPDLOG_LOGGER_ERROR(logger, "bad document: {}", e.what());
 * }
 *
 * @endcode
 *
 *
 */
class json_validator {
  rapidjson::SchemaDocument _schema;
  std::string _json_schema;
  friend class rapidjson_helper;

 public:
  json_validator(const json_validator&) = delete;
  json_validator& operator=(const json_validator&) = delete;

  json_validator(const std::string_view& json_schema);

  const std::string& get_json_schema() const { return _json_schema; }
};

namespace literals {

/**
 * @brief string literal to create rapidjson::Document
 * @code {.c++}
 * using com::centreon::common::literals;
 * rapidjson::Document doc = "R(
 * {
 *   "$schema": "http://json-schema.org/draft-07/schema#",
 *   "type": "object"
 *  }
 * )_json"
 *
 * @endcode
 *
 *
 */
inline rapidjson::Document operator"" _json(const char* s, std::size_t n) {
  return rapidjson_helper::read_from_string(std::string_view(s, n));
}

}  // namespace literals
}  // namespace com::centreon::common

namespace fmt {

template <>
struct formatter<rapidjson::Value> : formatter<std::string_view, char> {
  constexpr auto parse(format_parse_context& ctx) -> decltype(ctx.begin()) {
    return ctx.begin();
  }

  // Formats a rapidjson::Value
  template <typename FormatContext>
  auto format(const rapidjson::Value& val, FormatContext& ctx) const
      -> decltype(ctx.out()) {
    using namespace rapidjson;

    StringBuffer buffer;
    Writer<StringBuffer> writer(buffer);
    val.Accept(writer);

    return formatter<std::string_view, char>::format(
        {buffer.GetString(), buffer.GetLength()}, ctx);
  }
};
}  // namespace fmt

#endif
