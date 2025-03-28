/**
 * Copyright 2022-2025 Centreon (https://www.centreon.com/)
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

#ifndef CCE_CONFIGURATION_STATE
#define CCE_CONFIGURATION_STATE

#include <rapidjson/document.h>

#include "common/engine_conf/message_helper.hh"

namespace com::centreon::engine::configuration {

class state_helper : public message_helper {
  void _init();
  static void _expand_cv(configuration::State& s);

 public:
  state_helper(State* obj);
  ~state_helper() noexcept = default;

  bool hook(std::string_view key, std::string_view value) override;
  bool apply_extended_conf(const std::string& file_path,
                           const rapidjson::Document& json_doc,
                           const std::shared_ptr<spdlog::logger>& logger);
  bool set_global(const std::string_view& key, const std::string_view& value);
  void expand(configuration::error_cnt& err);
  static void diff(const State& old_state,
                   const State& new_state,
                   const std::shared_ptr<spdlog::logger>& logger,
                   DiffState* result);
};
}  // namespace com::centreon::engine::configuration

#endif /* !CCE_CONFIGURATION_STATE */
