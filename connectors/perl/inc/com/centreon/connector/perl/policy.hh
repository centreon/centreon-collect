/*
** Copyright 2022 Centreon
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

#ifndef CCCP_POLICY_HH
#define CCCP_POLICY_HH

#include "com/centreon/connector/ipolicy.hh"
#include "com/centreon/connector/perl/checks/check.hh"
#include "com/centreon/connector/perl/orders/parser.hh"
#include "com/centreon/connector/reporter.hh"

namespace com::centreon::connector::perl {

/**
 *  @class policy policy.hh "com/centreon/connector/perl/policy.hh"
 *  @brief Software policy.
 *
 *  Wraps software policy within a class.
 */
class policy : public com::centreon::connector::policy_interface {
  using pid_to_check_map = std::map<pid_t, checks::check::pointer>;
  pid_to_check_map _checks;
  reporter::pointer _reporter;
  shared_io_context _io_context;
  asio::system_timer _second_timer, _end_timer;

  policy(const shared_io_context& io_context);
  void start(const std::string& test_cmd_file);

  void on_sigchild();

  void start_end_timer(bool final);
  void start_second_timer();

  void wait_pid();

 public:
  policy(policy const& p) = delete;
  policy& operator=(policy const& p) = delete;

  std::shared_ptr<policy> shared_from_this() {
    return std::static_pointer_cast<policy>(
        com::centreon::connector::policy_interface::shared_from_this());
  }

  static void create(const shared_io_context& io_context,
                     const std::string& test_cmd_file);

  void on_eof() override;
  void on_error(uint64_t cmd_id, const std::string& msg) override;
  void on_execute(
      uint64_t cmd_id,
      const time_point& timeout,
      const std::shared_ptr<com::centreon::connector::orders::options>& opt)
      override;
  void on_quit() override;
  void on_version() override;
};

}  // namespace com::centreon::connector::perl

#endif  // !CCCP_POLICY_HH
