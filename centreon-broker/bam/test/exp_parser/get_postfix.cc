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

#include <gtest/gtest.h>
#include <list>
#include <string>
#include "com/centreon/broker/bam/exp_parser.hh"
#include "com/centreon/broker/exceptions/msg.hh"

using namespace com::centreon::broker;

/**
 *  Helper function.
 */
static std::list<std::string> array_to_list(char const* array[]) {
  std::list<std::string> retval;
  for (int i(0); array[i]; ++i)
    retval.push_back(array[i]);
  return (retval);
}

// Given an exp_parser object
// When it is constructed with a valid expression
// Then get_postfix() returns its postfix notation
TEST(BamExpParserGetPostfix, Valid1) {
  bam::exp_parser p("HOSTSTATUS(Host, Service) IS OK");
  char const* expected[] = {
    "Host",
    "Service",
    "HOSTSTATUS",
    "2",
    "OK",
    "IS",
    NULL
  };
  ASSERT_EQ(p.get_postfix(), array_to_list(expected));
}

// Given an exp_parser object
// When it is constructed with a valid expression
// Then get_postfix() return its postfix notation
TEST(BamExpParserGetPostfix, Valid2) {
  bam::exp_parser p("AVERAGE(METRICS('ping'), METRIC(\"my_ping\", Host, Service)) >= (SUM(METRICS('limit'), METRICS('limit_offset')) + 42)");
  char const* expected[] = {
    "ping",
    "METRICS",
    "1",
    "my_ping",
    "Host",
    "Service",
    "METRIC",
    "3",
    "AVERAGE",
    "2",
    "limit",
    "METRICS",
    "1",
    "limit_offset",
    "METRICS",
    "1",
    "SUM",
    "2",
    "42",
    "+",
    ">=",
    NULL
  };
  ASSERT_EQ(p.get_postfix(), array_to_list(expected));
}
