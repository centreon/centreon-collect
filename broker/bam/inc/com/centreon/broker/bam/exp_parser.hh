/*
** Copyright 2016 Centreon
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
**
** For more information : contact@centreon.com
*/

#ifndef CCB_BAM_EXP_PARSER_HH
#define CCB_BAM_EXP_PARSER_HH

#include "com/centreon/broker/namespace.hh"

CCB_BEGIN()

namespace bam {
/**
 *  @class exp_parser exp_parser.hh "com/centreon/broker/bam/exp_parser.hh"
 *  @brief Expression parser.
 *
 *  An expression parser for interpreting BAM expressions. More
 *  precisely it uses the shunting-yard algorithm to convert infix
 *  expressions to postfix. These should be later processed by
 *  the exp_builder to get a syntax tree.
 *
 *  @see exp_builder
 */
class exp_parser {
 public:
  using notation = std::list<std::string>;

 private:
  std::string _exp;
  absl::flat_hash_map<std::string, int> _precedence;
  notation _postfix;

 public:
  exp_parser(std::string const& expression);
  exp_parser(exp_parser const&) = delete;
  ~exp_parser() noexcept = default;
  exp_parser& operator=(const exp_parser&) = delete;
  notation const& get_postfix();
  static bool is_function(std::string const& token);
  static bool is_operator(std::string const& token);
};
}  // namespace bam

CCB_END()

#endif  // !CCB_BAM_EXP_PARSER_HH
