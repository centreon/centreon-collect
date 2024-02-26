/*
 * Copyright 2014, 2021-2023 Centreon
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

#ifndef CCB_BAM_BOOL_OPERATION_HH
#define CCB_BAM_BOOL_OPERATION_HH

#include "com/centreon/broker/bam/bool_binary_operator.hh"

namespace com::centreon::broker {

namespace bam {
/**
 *  @class bool_operation bool_operation.hh
 * "com/centreon/broker/bam/bool_operation.hh"
 *  @brief Boolean operation.
 *
 *  In the context of a KPI computation, bool_operation represents a
 *  mathematical operation between two bool_value.
 */
class bool_operation : public bool_binary_operator {
  enum operation_type {
    addition,
    substraction,
    multiplication,
    division,
    modulo
  };
  const operation_type _type;

 public:
  bool_operation(std::string const& op);
  ~bool_operation() noexcept override = default;
  bool_operation(const bool_operation&) = delete;
  bool_operation& operator=(const bool_operation&) = delete;
  double value_hard() const override;
  bool boolean_value() const override;
  bool state_known() const override;
  std::string object_info() const override;
};
}  // namespace bam

}  // namespace com::centreon::broker

#endif  // !CCB_BAM_BOOL_OR_HH
