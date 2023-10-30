/*
** Copyright 2011-2019 Centreon
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

#ifndef CCCS_POLICY_HH
#define CCCS_POLICY_HH

#include "com/centreon/connector/ipolicy.hh"
#include "com/centreon/connector/reporter.hh"
#include "com/centreon/connector/ssh/orders/options.hh"
#include "com/centreon/connector/ssh/sessions/credentials.hh"

CCCS_BEGIN()

// Forward declarations.

namespace sessions {
class session;
}

/**
 *  @class policy policy.hh "com/centreon/connector/ssh/policy.hh"
 *  @brief Software policy.
 *
 *  Manage program execution.
 */
class policy : public com::centreon::connector::policy_interface {
  bool _error;
  reporter::pointer _reporter;
  std::map<sessions::credentials, std::shared_ptr<sessions::session>> _sessions;

  struct connect_waiting_session {
    struct cmd_info {
      uint64_t cmd_id;
      time_point timeout;
      com::centreon::connector::orders::options::pointer opt;
    };

    std::queue<cmd_info> waiting_check;
    std::shared_ptr<sessions::session> _connecting;
  };

  std::map<sessions::credentials, connect_waiting_session>
      _connect_waiting_session;

  shared_io_context _io_context;

  policy(const shared_io_context& io_context);
  policy(policy const& p) = delete;
  policy& operator=(policy const& p) = delete;

  void start(const std::string& test_cmd_file);
  void on_connect(const boost::system::error_code& err,
                  const sessions::credentials& creds);

 public:
  using pointer = std::shared_ptr<policy>;

  pointer shared_from_this() {
    return std::static_pointer_cast<policy>(
        policy_interface::shared_from_this());
  }

  static pointer create(const shared_io_context& io_context,
                        const std::string& test_cmd_file);

  void on_eof() override;
  void on_error(uint64_t cmd_id, const std::string& msg) override;
  void on_execute(
      uint64_t cmd_id,
      const time_point& timeout,
      const com::centreon::connector::orders::options::pointer& opt) override;
  void on_execute(
      const std::shared_ptr<sessions::session>& session,
      uint64_t cmd_id,
      const time_point& timeout,
      const com::centreon::connector::orders::options::pointer& opt);
  void on_quit() override;
  void on_version() override;
};

CCCS_END()

#endif  // !CCCS_POLICY_HH
