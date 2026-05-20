/*
** Copyright 2026 Centreon
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

#include <unistd.h>
#include "com/centreon/connector/ipolicy.hh"
#include "com/centreon/connector/perl/orders/parser.hh"
#include "com/centreon/connector/reporter.hh"
#include "script_child.hh"
#include "src/perl_connector.pb.h"

namespace com::centreon::connector::perl {

class policy : public com::centreon::connector::policy_interface {
  struct script_path_extractor {
    using result_type = std::string;
    const result_type& operator()(
        const std::shared_ptr<script_child>& script_chld) const {
      return script_chld->get_script_path();
    }
  };

  using script_child_cont = boost::multi_index::multi_index_container<
      std::shared_ptr<script_child>,
      boost::multi_index::indexed_by<
          boost::multi_index::ordered_unique<script_path_extractor>,
          boost::multi_index::ordered_unique<
              boost::multi_index::identity<std::shared_ptr<script_child>>>>>;

  script_child_cont _scripts;
  absl::flat_hash_set<std::shared_ptr<script_child>> _dying_scripts;

  struct check_child_stat {
    check_child_stat() {}

    check_child_stat(const std::shared_ptr<script_child>& parent,
                     const Result& res);

    std::shared_ptr<script_child> parent;
    unsigned check_child_pid;
    time_t last_used;
    using footprint_type = std::tuple<unsigned /*used_memory*/,
                                      unsigned /*nb_opened_fd*/,
                                      unsigned /*nb_thread*/>;
    footprint_type footprint;
  };

  using check_child_stat_cont = boost::multi_index::multi_index_container<
      check_child_stat,
      boost::multi_index::indexed_by<
          boost::multi_index::ordered_non_unique<BOOST_MULTI_INDEX_MEMBER(
              check_child_stat,
              std::shared_ptr<script_child>,
              parent)>,
          boost::multi_index::ordered_unique<BOOST_MULTI_INDEX_MEMBER(
              check_child_stat,
              unsigned,
              check_child_pid)>,
          boost::multi_index::ordered_non_unique<
              BOOST_MULTI_INDEX_MEMBER(check_child_stat, time_t, last_used)>,
          boost::multi_index::ordered_non_unique<BOOST_MULTI_INDEX_MEMBER(
              check_child_stat,
              check_child_stat::footprint_type,
              footprint)>>>;

  check_child_stat_cont _check_child_stats;

  struct pending_query {
    pending_query() {}
    pending_query(uint64_t cmdid,
                  const std::string& cmdline,
                  time_point time_out,
                  const std::shared_ptr<script_child>& scriptworker)
        : cmd_id(cmdid),
          cmd_line(cmdline),
          timeout(time_out),
          script_worker(scriptworker) {}

    uint64_t cmd_id;
    std::string cmd_line;
    time_point timeout;
    std::shared_ptr<script_child> script_worker;
  };

  using pending_cont = boost::multi_index::multi_index_container<
      pending_query,
      boost::multi_index::indexed_by<
          boost::multi_index::ordered_unique<
              BOOST_MULTI_INDEX_MEMBER(pending_query, uint64_t, cmd_id)>,
          boost::multi_index::ordered_non_unique<
              BOOST_MULTI_INDEX_MEMBER(pending_query, time_point, timeout)>,
          boost::multi_index::ordered_non_unique<BOOST_MULTI_INDEX_MEMBER(
              pending_query,
              std::shared_ptr<script_child>,
              script_worker)>>>;

  pending_cont _pending_queries;

  reporter::pointer _reporter;
  const shared_io_context _io_context;
  const std::shared_ptr<spdlog::logger> _logger;
  const int _stdin_fd;
  asio::system_timer _every_second_timer;
  const config _config;
  char* _argv0;

  policy(const shared_io_context& io_context,
         const std::shared_ptr<spdlog::logger>& logger,
         const config& conf,
         char* argv0,
         int stdin_fd,
         int stdout_fd);
  void _start();

  void _from_script_child(std::shared_ptr<script_child> script_child,
                          const ConnectorMess& from_script_mess);

  void _on_script_child_end(std::shared_ptr<script_child> script_child);

  void _start_every_second_timer();
  void _every_second_timer_handler(const boost::system::error_code& err);

  ConnectorMess _create_execute(uint64_t cmd_id,
                                const time_point& timeout,
                                const std::string& cmdline,
                                bool no_child_create);

  size_t _free_memory(const std::shared_ptr<script_child>& who_need_memory);

  size_t _remove_heaviest_check_child();
  void _remove_oldest_check_child();

 public:
  policy(policy const& p) = delete;
  policy& operator=(policy const& p) = delete;
  ~policy();

  std::shared_ptr<policy> shared_from_this() {
    return std::static_pointer_cast<policy>(
        com::centreon::connector::policy_interface::shared_from_this());
  }

  static void create(const shared_io_context& io_context,
                     const std::shared_ptr<spdlog::logger>& logger,
                     const config& conf,
                     char* argv0,
                     int stdin_fd = STDIN_FILENO,
                     int stdout_fd = STDOUT_FILENO);

  void on_eof() override;
  void on_error(uint64_t cmd_id, const std::string& msg) override;
  void on_execute(uint64_t cmd_id,
                  const time_point& timeout,
                  const std::string& cmdline);
  void on_execute(
      uint64_t,
      const time_point&,
      const std::shared_ptr<com::centreon::connector::orders::options>&)
      override {}

  void on_quit() override;
  void on_version() override;
};

}  // namespace com::centreon::connector::perl

#endif
