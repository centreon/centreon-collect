/**
 * Copyright 2022 Centreon (https://www.centreon.com/)
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

#ifndef CCE_CONFIGURATION_APPLIER_TAG_HH
#define CCE_CONFIGURATION_APPLIER_TAG_HH
#include "common/engine_conf/tag_helper.hh"

namespace com::centreon::engine::configuration {

namespace applier {
class tag {
 public:
  tag() = default;
  ~tag() noexcept = default;
  tag& operator=(const tag& other) = delete;
  void add_object(const configuration::Tag& obj);
  void modify_object(configuration::Tag* to_modify,
                     const configuration::Tag& new_object);
  void remove_object(const std::pair<uint64_t, uint32_t>& key);
  void resolve_object(const configuration::Tag& obj, error_cnt& err);
};
}  // namespace applier
}  // namespace com::centreon::engine::configuration

#endif  // !CCE_CONFIGURATION_APPLIER_TAG_HH
