/**
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

#ifndef CCE_CONFIGURATION_TIMEPERIOD
#define CCE_CONFIGURATION_TIMEPERIOD

#include "common/configuration/message_helper.hh"
#include "common/configuration/state-generated.pb.h"
#include "common/configuration/state.pb.h"

namespace com {
namespace centreon {
namespace engine {
namespace configuration {

class timeperiod_helper : public message_helper {
  void _init();
  bool _add_calendar_date(const std::string& line);
  bool _add_other_date(const std::string& line);
  bool _build_timeranges(
      std::string const& line,
      google::protobuf::RepeatedPtrField<Timerange>& timeranges);
  bool _build_time_t(std::string_view time_str, time_t& ret);
  bool _get_day_id(std::string_view name, uint32_t& id);
  bool _get_month_id(std::string_view name, uint32_t& id);

 public:
  timeperiod_helper(Timeperiod* obj);
  ~timeperiod_helper() noexcept = default;
  void check_validity() const override;

  bool hook(absl::string_view key, const absl::string_view& value) override;
};

std::string daterange_to_str(const Daterange& dr);

}  // namespace configuration
}  // namespace engine
}  // namespace centreon
}  // namespace com

#endif /* !CCE_CONFIGURATION_TIMEPERIOD */
