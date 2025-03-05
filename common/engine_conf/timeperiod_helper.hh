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

#ifndef CCE_CONFIGURATION_TIMEPERIOD
#define CCE_CONFIGURATION_TIMEPERIOD

#include "common/engine_conf/message_helper.hh"
#include "common/engine_conf/state.pb.h"

namespace com::centreon::engine::configuration {

class timeperiod_helper : public message_helper {
  void _init();
  bool _add_week_day(std::string_view key, std::string_view value);
  bool _add_calendar_date(const std::string& line);
  bool _add_other_date(const std::string& line);
  bool _build_timeranges(
      std::string_view line,
      google::protobuf::RepeatedPtrField<Timerange>& timeranges);
  bool _build_time_t(std::string_view time_str, time_t& ret);
  bool _get_day_id(std::string_view name, uint32_t& id);
  bool _get_month_id(std::string_view name, uint32_t& id);

 public:
  timeperiod_helper(Timeperiod* obj);
  ~timeperiod_helper() noexcept = default;
  void check_validity(error_cnt& err) const override;

  bool hook(std::string_view key, std::string_view value) override;
};

std::string daterange_to_str(const Daterange& dr);

}  // namespace com::centreon::engine::configuration

#endif /* !CCE_CONFIGURATION_TIMEPERIOD */
