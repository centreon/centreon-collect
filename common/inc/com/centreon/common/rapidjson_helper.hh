/*
** Copyright 2024 Centreon
**
** This file is part of Centreon Engine.
**
** Centreon Engine is free software: you can redistribute it and/or
** modify it under the terms of the GNU General Public License version 2
** as published by the Free Software Foundation.
**
** Centreon Engine is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
** General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with Centreon Engine. If not, see
** <http://www.gnu.org/licenses/>.
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

  const char* get_string(const char* field_name) const;

  double get_double(const char* field_name) const;

  uint64_t get_uint64_t(const char* field_name) const;

  unsigned get_unsigned(const char* field_name) const;

  int get_int(const char* field_name) const;

  bool get_bool(const char* field_name) const;

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

  friend class rapidjson_helper;

 public:
  json_validator(const json_validator&) = delete;
  json_validator& operator=(const json_validator&) = delete;

  json_validator(const std::string_view& json_schema);
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
