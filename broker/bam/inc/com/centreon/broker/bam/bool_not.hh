/*
 * Copyright 2014,2016, 2023 Centreon
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

#ifndef CCB_BAM_BOOL_NOT_HH
#define CCB_BAM_BOOL_NOT_HH

#include "com/centreon/broker/bam/bool_value.hh"
#include "com/centreon/broker/io/stream.hh"
#include "com/centreon/broker/namespace.hh"

CCB_BEGIN()

namespace bam {
// Forward declaration.
class bool_value;

/**
 *  @class bool_not bool_not.hh "com/centreon/broker/bam/bool_not.hh"
 *  @brief NOT boolean operator.
 *
 *  In the context of a KPI computation, bool_not represents a logical
 *  NOT on a bool_value.
 */
class bool_not : public bool_value {
  bool_value::ptr _value;

 public:
  bool_not(bool_value::ptr val, const std::shared_ptr<spdlog::logger>& logger);
  bool_not(const bool_not&) = delete;
  ~bool_not() noexcept = default;
  bool_not& operator=(const bool_not&) = delete;
  void set_value(std::shared_ptr<bool_value>& value);
  double value_hard() const override;
  bool boolean_value() const override;
  double value_soft();
  bool state_known() const override;
  bool in_downtime() const override;
  void update_from(computable* child, io::stream* visitor) override;
  std::string object_info() const override;
  void dump(std::ofstream& output) const override;
};
}  // namespace bam

CCB_END()

#endif  // !CCB_BAM_BOOL_NOT_HH
