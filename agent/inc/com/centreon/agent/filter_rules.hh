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

const bp::symbols<label_compare_to_value::comparison> comparison_symbols = {
    {"<", label_compare_to_value::comparison::less_than},
    {"<=", label_compare_to_value::comparison::less_than_or_equal},
    {">", label_compare_to_value::comparison::greater_than},
    {">=", label_compare_to_value::comparison::greater_than_or_equal},
    {"!=", label_compare_to_value::comparison::not_equal},
    {"==", label_compare_to_value::comparison::equal},
    {"=", label_compare_to_value::comparison::equal}};

const auto label_compare_to_value_rule_def =
    (+bp::char_('a', 'z') >> *bp::ws >> comparison_symbols >> *bp::ws >>
     bp::double_ >> *bp::char_('a', 'z')) |
    (bp::double_ >> *bp::char_('a', 'z') >> *bp::ws >> comparison_symbols >>
     *bp::ws >> *bp::char_('a', 'z'));

BOOST_PARSER_DEFINE_RULES(label_compare_to_value_rule);

const bp::symbols<filter_combinator::logical_operator>
    logical_operator_symbols = {
        {"&&", filter_combinator::logical_operator::filter_and},
        {"||", filter_combinator::logical_operator::filter_or},
        {"and", filter_combinator::logical_operator::filter_and},
        {"or", filter_combinator::logical_operator::filter_or}};

const bp::rule<struct filter_combinator_rule1, filter_combinator>
    filter_combinator_rule1 = "filter_combinator";

const bp::rule<struct filter_combinator_rule2, filter_combinator>
    filter_combinator_rule2 = "filter_combinator";

const bp::rule<struct filter_combinator_rule, filter_combinator>
    filter_combinator_rule = "filter_combinator";

const auto filter_combinator_rule1_def =
    (label_compare_to_value_rule | filter_combinator_rule2) >>
    *(+bp::ws >> logical_operator_symbols >> +bp::ws >>
      (label_compare_to_value_rule | filter_combinator_rule2));

const auto filter_combinator_rule2_def = '(' >> filter_combinator_rule >> ')';

const auto filter_combinator_rule_def = (filter_combinator_rule1 |
                                         filter_combinator_rule2) >>
                                        *(logical_operator_symbols >>
                                          (filter_combinator_rule1 |
                                           filter_combinator_rule2));

BOOST_PARSER_DEFINE_RULES(filter_combinator_rule1);

BOOST_PARSER_DEFINE_RULES(filter_combinator_rule2);

// const auto filter_combinator_rule_def =
//     *bp::lit('(') >> (label_compare_to_value_rule | filter_combinator_rule)
//     >>
//     *(logical_operator_symbols >>
//       (label_compare_to_value_rule | filter_combinator_rule)) >>
//     *bp::lit(')');

BOOST_PARSER_DEFINE_RULES(filter_combinator_rule);

}  // namespace com::centreon::agent::filters

#pragma GCC diagnostic pop

#endif  // CENTREON_AGENT_FILTER__PARSER_HH
