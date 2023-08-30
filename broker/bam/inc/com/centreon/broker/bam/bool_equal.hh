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

#ifndef CCB_BAM_BOOL_EQUAL_HH
#define CCB_BAM_BOOL_EQUAL_HH

#include "com/centreon/broker/bam/bool_binary_operator.hh"

namespace com::centreon::broker {

namespace bam {
/**
 *  @class bool_or bool_or.hh "com/centreon/broker/bam/bool_or.hh"
 *  @brief OR operator.
 *
 *  In the context of a KPI computation, bool_or represents a logical
 *  egality between two bool_value.
 */
class bool_equal : public bool_binary_operator {
 public:
  bool_equal(const std::shared_ptr<spdlog::logger>& logger)
      : bool_binary_operator(logger) {}
  ~bool_equal() noexcept override = default;
  bool_equal(const bool_equal&) = delete;
  bool_equal& operator=(const bool_equal&) = delete;
  double value_hard() const override;
  bool boolean_value() const override;
  std::string object_info() const override;
};
}  // namespace bam

}

#endif  // !CCB_BAM_BOOL_EQUAL_HH
