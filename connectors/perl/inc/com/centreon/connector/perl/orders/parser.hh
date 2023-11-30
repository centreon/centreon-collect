/*
** Copyright 2011-2013 Centreon
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

#ifndef CCCP_ORDERS_PARSER_HH
#define CCCP_ORDERS_PARSER_HH

#include "com/centreon/connector/parser.hh"


Cnamespace com::centreon {

namespace orders {
class options : public std::string {
 public:
  options(const std::string& src) : std::string(src) {}
};
}  // namespace orders

C}()

CCCP_BEGIN()

namespace orders {
/**
 *  @class parser parser.hh "com/centreon/connector/perl/orders/parser.hh"
 *  @brief Parse orders.
 *
 *  Parse orders, generally issued by the monitoring engine. The
 *  parser class can handle be registered with one handle at a time
 *  and one listener.
 */
class parser : public com::centreon::connector::parser {
 protected:
  parser(const shared_io_context& io_context,
         const std::shared_ptr<com::centreon::connector::policy_interface>&
             policy);

  void execute(const std::string& cmd) override;

 public:
  using pointer = std::shared_ptr<parser>;

  static pointer create(shared_io_context io_context,
                        const std::shared_ptr<policy_interface>& policy,
                        const std::string& test_cmd_file = "");

  parser(parser const& p) = delete;
  parser& operator=(parser const& p) = delete;
};
}  // namespace orders

CCCP_END()

#endif  // !CCCP_ORDERS_PARSER_HH
