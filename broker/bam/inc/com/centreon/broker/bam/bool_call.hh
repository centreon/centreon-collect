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

#ifndef CCB_BAM_BOOL_CALL_HH
#define CCB_BAM_BOOL_CALL_HH

#include "com/centreon/broker/bam/bool_expression.hh"
#include "com/centreon/broker/bam/bool_value.hh"

namespace com::centreon::broker::bam {
/**
 *  @class bool_call bool_call.hh "com/centreon/broker/bam/bool_call.hh"
 *  @brief Bool Call.
 *
 *  In the context of a KPI computation, bool_call represents a call
 *  to an external expression.
 */
class bool_call : public bool_value {
  std::string _name;
  std::shared_ptr<bool_value> _expression;

 public:
  typedef std::shared_ptr<bool_call> ptr;

  bool_call(std::string const& name,
            const std::shared_ptr<spdlog::logger>& logger);
  ~bool_call() noexcept override = default;
  bool_call(const bool_call&) = delete;
  bool_call& operator=(const bool_call&) = delete;
  double value_hard() const override;
  bool boolean_value() const override;
  bool state_known() const override;
  std::string const& get_name() const;
  void set_expression(std::shared_ptr<bool_value> expression);
  void update_from(computable* child,
                   io::stream* visitor,
                   const std::shared_ptr<spdlog::logger>& logger) override;
};
}  // namespace com::centreon::broker::bam

#endif  // !CCB_BAM_BOOL_CALL_HH
