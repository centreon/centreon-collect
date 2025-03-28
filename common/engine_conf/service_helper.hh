/**
 * Copyright 2022-2024 Centreon (https://www.centreon.com/)
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

#ifndef CCE_CONFIGURATION_SERVICE
#define CCE_CONFIGURATION_SERVICE

#include "common/engine_conf/message_helper.hh"

namespace com::centreon::engine::configuration {

class service_helper : public message_helper {
  void _init();

 public:
  service_helper(Service* obj);
  ~service_helper() noexcept = default;
  void check_validity(error_cnt& err) const override;

  bool hook(std::string_view key, std::string_view value) override;

  bool insert_customvariable(std::string_view key,
                             std::string_view value) override;
  static void expand(
      configuration::State& s,
      configuration::error_cnt& err,
      const absl::flat_hash_map<std::string_view, const configuration::Host*>&
          m_host,
      const absl::flat_hash_map<std::string_view, configuration::Servicegroup*>&
          sgs);
};
}  // namespace com::centreon::engine::configuration

#endif /* !CCE_CONFIGURATION_SERVICE */
