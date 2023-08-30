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

#ifndef CCB_BAM_BOOL_EXPRESSION_HH
#define CCB_BAM_BOOL_EXPRESSION_HH

#include "bbdo/bam/state.hh"
#include "com/centreon/broker/bam/computable.hh"
#include "com/centreon/broker/io/stream.hh"
#include "com/centreon/broker/namespace.hh"
#include "impact_values.hh"

CCB_BEGIN()

namespace bam {
// Forward declaration.
class bool_value;

/**
 *  @class bool_expression bool_expression.hh
 * "com/centreon/broker/bam/bool_expression.hh"
 *  @brief Boolean expression.
 *
 *  Stores and entire boolean expression made of multiple boolean
 *  operations and evaluate them to match the kpi interface.
 */
class bool_expression : public computable {
  const uint32_t _id;
  const bool _impact_if;
  std::shared_ptr<bool_value> _expression;

 public:
  bool_expression(uint32_t id,
                  bool impact_if,
                  const std::shared_ptr<spdlog::logger>& logger);
  bool_expression(const bool_expression&) = delete;
  ~bool_expression() noexcept override = default;
  bool_expression& operator=(const bool_expression&) = delete;
  state get_state() const;
  bool state_known() const;
  void set_expression(std::shared_ptr<bool_value> const& expression);
  std::shared_ptr<bool_value> get_expression() const;
  bool in_downtime() const;
  uint32_t get_id() const;
  void update_from(computable* child, io::stream* visitor) override;
  std::string object_info() const override;
  void dump(std::ofstream& output) const override;
};
}  // namespace bam

CCB_END()

#endif  // !CCB_BAM_BOOL_EXPRESSION_HH
