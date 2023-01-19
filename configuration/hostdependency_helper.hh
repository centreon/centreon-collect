/*
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

#ifndef CCE_CONFIGURATION_HOSTDEPENDENCY
#define CCE_CONFIGURATION_HOSTDEPENDENCY

#include "configuration/message_helper.hh"
#include "configuration/state-generated.pb.h"

namespace com {
namespace centreon {
namespace engine {
namespace configuration {

class hostdependency_helper : public message_helper {
 public:
  hostdependency_helper(Hostdependency* obj);
  ~hostdependency_helper() noexcept = default;

  bool hook(const absl::string_view& key,
            const absl::string_view& value) override;
  void check_validity() const override;
};
}  // namespace configuration
}  // namespace engine
}  // namespace centreon
}  // namespace com

#endif /* !CCE_CONFIGURATION_HOSTDEPENDENCY */