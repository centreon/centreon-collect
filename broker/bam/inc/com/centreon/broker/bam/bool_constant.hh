/*
 * Copyright 2014, 2023 Centreon
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

#ifndef CCB_BAM_BOOL_CONSTANT_HH
#define CCB_BAM_BOOL_CONSTANT_HH

#include "com/centreon/broker/bam/bool_value.hh"

namespace com::centreon::broker::bam {
/**
 *  @class bool_and bool_and.hh "com/centreon/broker/bam/bool_and.hh"
 *  @brief AND operator.
 *
 *  In the context of a KPI computation, bool_constant represents a constant
 *  value (i.e '42').
 */
class bool_constant : public bool_value {
  const double _value;
  /* Same value as _value but seen as boolean. */
  const bool _boolean_value;

 public:
  bool_constant(double value, const std::shared_ptr<spdlog::logger>& logger);
  ~bool_constant() noexcept override = default;
  bool_constant(const bool_constant&) = delete;
  bool_constant& operator=(const bool_constant&) = delete;
  double value_hard() const override;
  bool boolean_value() const override;
  bool state_known() const override;
  void update_from(computable* child,
                   io::stream* visitor,
                   const std::shared_ptr<spdlog::logger>& logger) override;
  std::string object_info() const override;
  void dump(std::ofstream& output) const override;
};
}  // namespace com::centreon::broker::bam

#endif  // !CCB_BAM_BOOL_CONSTANT_HH
