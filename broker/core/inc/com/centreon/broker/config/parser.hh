/**
 * Copyright 2011-2013,2015,2017-2023 Centreon
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

#ifndef CCB_CONFIG_PARSER_HH
#define CCB_CONFIG_PARSER_HH

#include <absl/types/optional.h>

#include "com/centreon/broker/config/state.hh"
#include "com/centreon/exceptions/msg_fmt.hh"

namespace com::centreon::broker::config {
/**
 *  @class parser parser.hh "com/centreon/broker/config/parser.hh"
 *  @brief Parse configuration file.
 *
 *  Parse a configuration file and generate appropriate objects for further
 *  handling.
 */
class parser {
  void _get_generic_endpoint_configuration(const nlohmann::json& elem,
                                           endpoint& e);
  void _parse_endpoint(const nlohmann::json& elem,
                       endpoint& e,
                       std::string& module);

 public:
  /**
   * @brief Check if the json object elem contains an entry with the given key.
   *        If it contains it, it gets it and returns it as a T. In case of
   *        error, it throws an exception.
   *        If the key doesn't exist, nothing is returned, that's the reason we
   *        use absl::optional<>.
   *
   * @param elem The json object to work with.
   * @param key  the key to obtain the value.
   *
   * @return an absl::optional<T> containing the wanted value or empty if
   *         nothing found.
   */
  template <typename T>
  static absl::optional<T> check_and_read(const nlohmann::json& elem,
                                          const std::string& key) {
    if (elem.contains(key)) {
      auto& ret = elem[key];
      if (ret.is_number())
        return {ret};
      else if (ret.is_string()) {
        T tmp;
        if (!absl::SimpleAtoi(ret.get<std::string_view>(), &tmp))
          throw exceptions::msg_fmt(
              "config parser: cannot parse key '{}': "
              "the string value must contain an integer",
              key);
        return {tmp};
      } else
        throw exceptions::msg_fmt(
            "config parser: cannot parse key '{}': "
            "the content must be an integer",
            key);
    }
    return absl::nullopt;
  }

  parser() = default;
  ~parser() noexcept = default;
  parser(const parser&) = delete;
  parser& operator=(const parser&) = delete;
  state parse(const std::string& file);
};

template <>
absl::optional<std::string> parser::check_and_read<std::string>(
    const nlohmann::json& elem,
    const std::string& key);

template <>
absl::optional<bool> parser::check_and_read<bool>(const nlohmann::json& elem,
                                                  const std::string& key);
}  // namespace com::centreon::broker::config

#endif  // !CCB_CONFIG_PARSER_HH
