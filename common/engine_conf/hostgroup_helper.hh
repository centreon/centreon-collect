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
#ifndef CCE_CONFIGURATION_HOSTGROUP
#define CCE_CONFIGURATION_HOSTGROUP

#include "common/engine_conf/message_helper.hh"
#include "common/engine_conf/state.pb.h"

namespace com::centreon::engine::configuration {

/**
 * @brief Helper for the Hostgroup message. The helper is instanciated
 * just after a message is created. It provides default values for it and also
 * several methods to help the developer to fill the message fields.
 */
class hostgroup_helper : public message_helper {
  void _init();

 public:
  hostgroup_helper(Hostgroup* obj);
  ~hostgroup_helper() noexcept = default;
  void check_validity(error_cnt& err) const override;

  bool hook(std::string_view key, const std::string_view& value) override;
};
}  // namespace com::centreon::engine::configuration

#endif /* !CCE_CONFIGURATION_HOSTGROUP */
