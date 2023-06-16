/*
 * Copyright 2022-2023 Centreon (https://www.centreon.com/)
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

#ifndef CCE_CONFIGURATION_SERVICEGROUP
#define CCE_CONFIGURATION_SERVICEGROUP

#include "configuration/message_helper.hh"
#include "configuration/state-generated.pb.h"

namespace com {
namespace centreon {
namespace engine {
namespace configuration {

class servicegroup_helper : public message_helper {
  void _init();

 public:
  servicegroup_helper(Servicegroup* obj);
  ~servicegroup_helper() noexcept = default;
  void check_validity() const override;

  bool hook(absl::string_view key, const absl::string_view& value) override;
};
}  // namespace configuration
}  // namespace engine
}  // namespace centreon
}  // namespace com

#endif /* !CCE_CONFIGURATION_SERVICEGROUP */