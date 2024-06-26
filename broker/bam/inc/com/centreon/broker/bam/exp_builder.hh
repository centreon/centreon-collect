/**
 * Copyright 2016, 2024 Centreon
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

#ifndef CCB_BAM_EXP_BUILDER_HH
#define CCB_BAM_EXP_BUILDER_HH

#include "com/centreon/broker/bam/bool_call.hh"
#include "com/centreon/broker/bam/bool_service.hh"
#include "com/centreon/broker/bam/exp_parser.hh"
#include "com/centreon/broker/bam/hst_svc_mapping.hh"

namespace com::centreon::broker::bam {
/**
 *  @class exp_builder exp_builder.hh "com/centreon/broker/bam/exp_builder.hh"
 *  @brief Convert expression to syntax tree.
 *
 *  Build a syntax tree from the postfix notation of an expression and
 *  return hooking elements.
 */
class exp_builder {
 public:
  using list_call = std::list<bool_call::ptr>;
  using list_service = std::list<bool_service::ptr>;
  using any_operand = std::pair<bool_value::ptr, std::string>;

 private:
  std::shared_ptr<spdlog::logger> _logger;
  hst_svc_mapping const& _mapping;
  list_call _calls;
  list_service _services;
  std::stack<any_operand> _operands;
  bool_value::ptr _tree;

  void _check_arity(std::string const& func, int expected, int given);
  bool _is_static_function(std::string const& str) const;
  bool_value::ptr _pop_operand();
  std::string _pop_string();

 public:
  exp_builder(exp_parser::notation const& postfix,
              hst_svc_mapping const& mapping,
              const std::shared_ptr<spdlog::logger>& logger);
  exp_builder(const exp_builder&) = delete;
  exp_builder& operator=(const exp_builder&);
  ~exp_builder() noexcept = default;
  list_call const& get_calls() const;
  list_service const& get_services() const;
  bool_value::ptr get_tree() const;
};
}  // namespace com::centreon::broker::bam

#endif  // !CCB_BAM_EXP_BUILDER_HH
