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

#ifndef CCCP_CHECKS_CHECK_HH
#define CCCP_CHECKS_CHECK_HH

namespace com::centreon::connector {
class result;
class reporter;
}  // namespace com::centreon::connector

namespace com::centreon::connector::perl {

namespace checks {

using shared_signal_set = std::shared_ptr<asio::signal_set>;

/**
 *  @class check check.hh "com/centreon/connector/perl/checks/check.hh"
 *  @brief Perl check.
 *
 *  Class wrapping a Perl check as requested by the monitoring engine.
 */
class check : public std::enable_shared_from_this<check> {
 public:
  using pointer = std::shared_ptr<check>;

  check(uint64_t cmd_id,
        const std::string& cmds,
        const time_point& tmt,
        const std::shared_ptr<com::centreon::connector::reporter> reporter,
        const shared_io_context& io_context);
  ~check();
  check(check const& c) = delete;
  check& operator=(check const& c) = delete;

  void on_timeout(const boost::system::error_code& err, bool final);

  void dump(std::ostream& s) const;

  void set_exit_code(int exit_code);

  pid_t execute();

  static void close_all_father_fd();
  static unsigned get_nb_check() { return _active_check.size(); }

 private:
  void _start_read_out();
  void _start_read_err();
  void _send_result();

  static constexpr size_t buff_size = 4096;
  using recv_buff = std::array<char, buff_size>;

  recv_buff _out_buff, _err_buff;

  pid_t _child;
  uint64_t _cmd_id;
  std::string _cmd;
  asio::readable_pipe _out, _err;
  int _out_fd, _err_fd;
  std::string _stderr;
  std::string _stdout;
  time_point _timeout;
  bool _out_eof, _err_eof, _exit_code_set;
  int _exit_code;
  asio::system_timer _timeout_timer;
  std::shared_ptr<com::centreon::connector::reporter> _reporter;
  shared_io_context _io_context;

  static absl::flat_hash_set<int> _all_child_fd;
  static absl::flat_hash_set<check*> _active_check;
  static std::vector<shared_signal_set> _father_signal_set;
};

inline std::ostream& operator<<(std::ostream& s, const check& obj) {
  obj.dump(s);
  return s;
}

}  // namespace checks

}  // namespace com::centreon::connector::perl

namespace fmt {
// formatter specializations for fmt
template <>
struct formatter<com::centreon::connector::perl::checks::check>
    : ostream_formatter {};
}  // namespace fmt

#endif  // !CCCP_CHECKS_CHECK_HH
