/*
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

#ifndef CCE_CONFIGURATION_APPLIER_SEVERITY_HH
#define CCE_CONFIGURATION_APPLIER_SEVERITY_HH
#include "common/engine_conf/severity_helper.hh"

namespace com::centreon::engine::configuration::applier {

class severity {
 public:
  severity() = default;
  ~severity() noexcept = default;
  severity& operator=(const severity& other) = delete;
  void add_object(const configuration::Severity& obj);
  void modify_object(configuration::Severity* to_modify,
                     const configuration::Severity& new_object);
  void remove_object(const std::pair<uint64_t, uint32_t>& p);
  void resolve_object(const configuration::Severity& obj, error_cnt& err);
};
}  // namespace com::centreon::engine::configuration::applier

#endif  // !CCE_CONFIGURATION_APPLIER_SEVERITY_HH
