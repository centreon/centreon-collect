/*
 * Copyright 20222023 Centreon
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * For more information : contact@centreon.com
 */

#ifndef CCB_BAM_BA_RATIO_NUMBER_HH
#define CCB_BAM_BA_RATIO_NUMBER_HH

#include "com/centreon/broker/bam/ba.hh"

namespace com::centreon::broker::bam {
// Forward declaration.
class kpi;

/**
 *  @class ba ba.hh "com/centreon/broker/bam/ba.hh"
 *  @brief Business activity.
 *
 *  Represents a BA that gets computed every time an impact change
 *  of value.
 */
class ba_ratio_number : public ba {
  bool _apply_impact(kpi* kpi_ptr, impact_info& impact) override;
  void _unapply_impact(kpi* kpi_ptr, impact_info& impact) override;
  void _recompute() override;

 public:
  ba_ratio_number(uint32_t id,
                  uint32_t host_id,
                  uint32_t service_id,
                  bool generate_virtual_status,
                  const std::shared_ptr<spdlog::logger>& logger);
  state get_state_hard() const override;
  state get_state_soft() const override;
  std::string get_output() const override;
  std::string get_perfdata() const override;
};
}  // namespace com::centreon::broker::bam

#endif  // !CCB_BAM_BA_RATIO_NUMBER_HH
