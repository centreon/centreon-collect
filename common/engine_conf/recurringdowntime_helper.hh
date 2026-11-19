/**
 * Copyright 2026 Centreon (https://www.centreon.com/)
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

#ifndef CCE_CONFIGURATION_RECURRINGDOWNTIME_HH
#define CCE_CONFIGURATION_RECURRINGDOWNTIME_HH

#include "common/engine_conf/message_helper.hh"
#include "common/engine_conf/state.pb.h"

namespace com::centreon::engine::configuration {

/**
 * @brief Helper of the recurringdowntime object: parses `period`, `host_ids`
 * and `service_ids`, and checks that the rule has an id, a period and a
 * target.
 */
class recurringdowntime_helper : public message_helper {
 public:
  recurringdowntime_helper(Recurringdowntime* obj);
  ~recurringdowntime_helper() noexcept = default;
  void check_validity(error_cnt& err) const override;

  bool hook(std::string_view key, std::string_view value) override;
  void set_default_values() override {}

  static bool parse_period(std::string_view value,
                           RecurringdowntimePeriod* period);
};
}  // namespace com::centreon::engine::configuration

#endif /* !CCE_CONFIGURATION_RECURRINGDOWNTIME_HH */
