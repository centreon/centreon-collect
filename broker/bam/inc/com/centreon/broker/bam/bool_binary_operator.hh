/*
 * Copyright 2014-2016, 2021-2023 Centreon
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

#ifndef CCB_BAM_BOOL_BINARY_OPERATOR_HH
#define CCB_BAM_BOOL_BINARY_OPERATOR_HH

#include "com/centreon/broker/bam/bool_value.hh"
#include "com/centreon/broker/io/stream.hh"

#define COMPARE_EPSILON 0.0001

namespace com::centreon::broker {

namespace bam {
/**
 *  @class bool_binary_operator bool_binary_operator.hh
 * "com/centreon/broker/bam/bool_binary_operator.hh"
 *  @brief Abstracts a binary boolean operator (AND, OR, XOR).
 *
 *  Provides common methods to binary operators.
 */
class bool_binary_operator : public bool_value {
 protected:
  std::shared_ptr<bool_value> _left;
  std::shared_ptr<bool_value> _right;
  double _left_hard = 0;
  double _right_hard = 0;
  bool _in_downtime = false;
  bool _state_known = false;

  virtual void _update_state();

 public:
  typedef std::shared_ptr<bool_binary_operator> ptr;

  bool_binary_operator() = default;
  bool_binary_operator(bool_binary_operator const&) = delete;
  ~bool_binary_operator() noexcept override = default;
  bool_binary_operator& operator=(const bool_binary_operator&) = delete;
  void set_left(const std::shared_ptr<bool_value>& left);
  void set_right(std::shared_ptr<bool_value> const& right);
  bool state_known() const override;
  bool in_downtime() const override;
  void update_from(computable* child, io::stream* visitor) override;
  void dump(std::ofstream& output) const override;
};
}  // namespace bam

}

#endif  // !CCB_BAM_BOOL_BINARY_OPERATOR_HH
