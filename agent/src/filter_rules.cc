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
 *
 *
 * This file is designed to be included as a header in final parser file
 *
 */

#include "filter_rules.hh"

namespace com::centreon::agent::filters {

namespace bp = boost::parser;

// Grammar elements are described here:
// https://www.boost.org/doc/libs/1_87_0/doc/html/boost_parser/cheat_sheet.html

/************************************************************************
label_compare_to_value grammar

We expect a string like 5.5<unit> <= label or label >= 5.5<unit> with or not
whitespace
labels should be a lower case string as units

**************************************************************************/

/**
 * @brief accepted operators
 *
 */
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

/************************************************************************
label_compare_to_string grammar

We expect a string likeprovider = 'Microsoft-Windows-Security-SPP'
labels should be a lower case string

**************************************************************************/

/**
 * @brief accepted operators
 *
 */
const bp::symbols<string_comparison> str_comparison_symbols = {
    {"!=", string_comparison::not_equal},
    {"==", string_comparison::equal},
    {"=", string_comparison::equal}};

const auto label_compare_to_string_rule_def = +bp::char_('a', 'z') >> *bp::ws >>
                                              str_comparison_symbols >>
                                              *bp::ws >> bp::quoted_string("'");

/************************************************************************
label_in grammar

We expect a string like label in (value1, 'value2', value3) or label not_in
(value1, 'value2', value3) with or not whitespace

label should be in lowercase as no quoted values

**************************************************************************/

const bp::symbols<in_not> in_symbols = {{"in", in_not::in},
                                        {"not_in", in_not::not_in}};

const auto label_in_rule_def =
    +bp::char_('a', 'z') >> *bp::ws >> in_symbols >> *bp::ws >> '(' >>
    *bp::ws >> (bp::quoted_string("'\"") | +bp::char_('a', 'z')) >>
    *(*bp::ws >> ',' >> *bp::ws >>
      (bp::quoted_string("'\"") | +bp::char_('a', 'z'))) >>
    *bp::ws >> ')';

/************************************************************************
filter_combinator grammar

This filter checks nothing, it is just a combinator of filters
It accepts parents to prioritize the order of the filters
If you mix and and or, the and will be prioritized
This grammar is recursive as you can have multiple levels of filters
As we accepts parent, we have two sub grammar, a first that contains only some
toto && titi || tutu And a second one that accept '('toto && titi || tutu ')'

Then the mother rule uses an or of the two sub rules to create the final grammar

**************************************************************************/

const bp::symbols<filter_combinator::logical_operator>
    logical_operator_symbols = {
        {"&&", filter_combinator::logical_operator::filter_and},
        {"||", filter_combinator::logical_operator::filter_or},
        {"and", filter_combinator::logical_operator::filter_and},
        {"or", filter_combinator::logical_operator::filter_or}};

/**
 * @brief beware to the orders, we first try to decode a filter before decoding
 * a sub combinator
 *
 */
const auto filter_combinator_rule1_def =
    (label_compare_to_value_rule | label_compare_to_string_rule |
     label_in_rule | filter_combinator_rule2) >>
    *(+bp::ws >> logical_operator_symbols >> +bp::ws >>
      (label_compare_to_value_rule | label_compare_to_string_rule |
       label_in_rule | filter_combinator_rule2));

const auto filter_combinator_rule2_def =
    '(' >> *bp::ws >> filter_combinator_rule >> *bp::ws >> ')';

const auto filter_combinator_rule_def = (filter_combinator_rule1 |
                                         filter_combinator_rule2) >>
                                        *(logical_operator_symbols >>
                                          (filter_combinator_rule1 |
                                           filter_combinator_rule2));

/**
 * wchar_t version
 */

const auto label_compare_to_string_rule_w_def = +bp::char_('a', 'z') >>
                                                *bp::ws >>
                                                str_comparison_symbols >>
                                                *bp::ws >>
                                                bp::quoted_string("'");

const auto label_in_rule_w_def =
    +bp::char_('a', 'z') >> *bp::ws >> in_symbols >> *bp::ws >> '(' >>
    *bp::ws >> (bp::quoted_string("'\"") | +bp::char_('a', 'z')) >>
    *(*bp::ws >> ',' >> *bp::ws >>
      (bp::quoted_string("'\"") | +bp::char_('a', 'z'))) >>
    *bp::ws >> ')';

const auto filter_combinator_rule1_w_def =
    (label_compare_to_value_rule | label_compare_to_string_rule_w |
     label_in_rule_w | filter_combinator_rule2_w) >>
    *(+bp::ws >> logical_operator_symbols >> +bp::ws >>
      (label_compare_to_value_rule | label_compare_to_string_rule_w |
       label_in_rule_w | filter_combinator_rule2_w));

const auto filter_combinator_rule2_w_def =
    '(' >> *bp::ws >> filter_combinator_rule_w >> *bp::ws >> ')';

const auto filter_combinator_rule_w_def = (filter_combinator_rule1_w |
                                           filter_combinator_rule2_w) >>
                                          *(logical_operator_symbols >>
                                            (filter_combinator_rule1_w |
                                             filter_combinator_rule2_w));

BOOST_PARSER_DEFINE_RULES(label_compare_to_value_rule,
                          label_compare_to_string_rule,
                          label_compare_to_string_rule_w,
                          label_in_rule,
                          label_in_rule_w,
                          filter_combinator_rule1,
                          filter_combinator_rule2,
                          filter_combinator_rule,
                          filter_combinator_rule1_w,
                          filter_combinator_rule2_w,
                          filter_combinator_rule_w);

};  // namespace com::centreon::agent::filters