/**
 * Copyright 2024 Centreon
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

#ifndef CENTREON_AGENT_FILTER_PARSER_HH
#define CENTREON_AGENT_FILTER_PARSER_HH

#include "filter.hh"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"

#include <boost/parser/parser.hpp>

namespace com::centreon::agent::filters {

namespace bp = boost::parser;

const bp::rule<struct label_compare_to_value_rule, label_compare_to_value>
    label_compare_to_value_rule = "label_compare_to_value";

const bp::rule<struct label_in_rule, label_in<char>> label_in_rule = "label_in";

const bp::rule<struct filter_combinator_rule1, filter_combinator>
    filter_combinator_rule1 = "filter_combinator1";

const bp::rule<struct filter_combinator_rule2, filter_combinator>
    filter_combinator_rule2 = "filter_combinator2";

const bp::rule<struct filter_combinator_rule, filter_combinator>
    filter_combinator_rule = "filter_combinator";

/**
 * @brief the same for wchar_t
 *
 */
const bp::rule<struct label_in_rule_w, label_in<wchar_t>> label_in_rule_w =
    "label_in";

const bp::rule<struct filter_combinator_rule1_w, filter_combinator>
    filter_combinator_rule1_w = "filter_combinator1";

const bp::rule<struct filter_combinator_rule2_w, filter_combinator>
    filter_combinator_rule2_w = "filter_combinator2";

const bp::rule<struct filter_combinator_rule_w, filter_combinator>
    filter_combinator_rule_w = "filter_combinator";

}  // namespace com::centreon::agent::filters

#pragma GCC diagnostic pop

#endif  // CENTREON_AGENT_FILTER__PARSER_HH
