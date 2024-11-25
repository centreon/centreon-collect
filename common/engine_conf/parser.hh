/**
 * Copyright 2011-2013,2017-2024 Centreon
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
#ifndef CCE_CONFIGURATION_PARSER_HH
#define CCE_CONFIGURATION_PARSER_HH

#include <filesystem>
#include "common/engine_conf/message_helper.hh"
#include "state_helper.hh"

namespace com::centreon::engine::configuration {

/**
 * @brief Each instance of a pb_map_object is about one type of message, for
 * example it contains only commands. It is just a map containing commands
 * indexed by their name.
 */
using pb_map_object =
    absl::flat_hash_map<std::string, std::unique_ptr<Message>>;

/**
 * @brief A Protobuf message has only predefined default values, 0 for integers,
 * empty for an array, etc. And we cannot change these default values. Because
 * we still work with cfg files, we also have some tricks when reading values,
 * some arrays are stored as strings, some bitfields are also stored as strings,
 * etc. So we need a helper to proceed in these operations. This is what is a
 * message_helper. Each protobuf configuration message has its own helper. The
 * map below makes the relation between a new message and its helper.
 */
using pb_map_helper =
    absl::flat_hash_map<Message*, std::unique_ptr<message_helper>>;

class parser {
  std::shared_ptr<spdlog::logger> _logger;

  /**
   * @brief An array of pb_map_objects. At index object_type::command we get all
   * the templates of commands, at index object_type::service we get all the
   * templates of services, etc.
   */
  std::array<pb_map_object, message_helper::object_type::nb_types>
      _pb_templates;

  /**
   * @brief The map of helpers of all the configuration objects parsed by this
   * parser.
   */
  pb_map_helper _pb_helper;

  unsigned int _current_line;
  std::string _current_path;

  /**
   *  Apply parse method into list.
   *
   *  @param[in] lst   The list to apply action.
   *  @param[in] pfunc The method to apply.
   */
  template <typename L>
  void _apply(const L& lst,
              State* pb_config,
              void (parser::*pfunc)(const std::string&, State*)) {
    for (auto& f : lst)
      (this->*pfunc)(f, pb_config);
  }
  void _parse_directory_configuration(std::string const& path,
                                      State* pb_config);
  void _parse_global_configuration(const std::string& path, State* pb_config);
  void _parse_object_definitions(const std::string& path, State* pb_config);
  void _parse_resource_file(std::string const& path, State* pb_config);
  void _resolve_template(State* pb_config, error_cnt& err);
  void _resolve_template(std::unique_ptr<message_helper>& msg_helper,
                         const pb_map_object& tmpls);
  void _merge(std::unique_ptr<message_helper>& msg_helper, Message* tmpl);
  void _cleanup(State* pb_config);

 public:
  parser();
  parser(const parser&) = delete;
  parser& operator=(const parser&) = delete;
  ~parser() noexcept = default;
  void parse(const std::string& path, State* config, error_cnt& err);
};
}  // namespace com::centreon::engine::configuration

#endif  // !CCE_CONFIGURATION_PARSER_HH
