/**
 * Copyright 2011 - 2019 Centreon (https://www.centreon.com/)
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * For more information : contact@centreon.com
 *
 */

#ifndef CCB_STATS_PARSER_HH
#define CCB_STATS_PARSER_HH

namespace com::centreon::broker {

namespace stats {
// Forward declaration.
class config;

/**
 *  @class parser parser.hh "com/centreon/broker/stats/parser.hh"
 *  @brief Parse a <stats> node.
 *
 *  Parse the <stats> node.
 */
class parser {
 public:
  parser();
  ~parser() throw();

  parser(parser const& right) = delete;
  parser& operator=(parser const& right) = delete;

  void parse(std::vector<std::string>& cfg, std::string const& content);
};
}  // namespace stats

}  // namespace com::centreon::broker

#endif  // !CCB_STATS_PARSER_HH
