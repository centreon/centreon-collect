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

#include <cstdint>
#include "com/centreon/connector/ipolicy.hh"
#include "com/centreon/connector/perl/config.hh"
#include "com/centreon/connector/perl/orders/parser.hh"
#include "com/centreon/connector/reporter.hh"
#include "script_child.hh"
#include "src/perl_connector.pb.h"

namespace com::centreon::connector::perl {

class policy : public com::centreon::connector::policy_interface {
  absl::flat_hash_map<std::string, std::shared_ptr<script_child>> _scripts;
  std::vector<std::shared_ptr<script_child>> _dying_scripts;

  struct check_child_stat {
    std::shared_ptr<script_child> parent;
    unsigned pid;
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
          boost::multi_index::ordered_unique<
              BOOST_MULTI_INDEX_MEMBER(check_child_stat, unsigned, pid)>,
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
                  time_point time_out)
        : cmd_id(cmdid), cmd_line(cmdline), timeout(time_out) {}

    uint64_t cmd_id;
    std::string cmd_line;
    time_point timeout;
  };

  using pending_cont = boost::multi_index::multi_index_container<
      pending_query,
      boost::multi_index::indexed_by<
          boost::multi_index::ordered_unique<
              BOOST_MULTI_INDEX_MEMBER(pending_query, uint64_t, cmd_id)>,
          boost::multi_index::ordered_non_unique<
              BOOST_MULTI_INDEX_MEMBER(pending_query, time_point, timeout)>>>;

  pending_cont _pending_queries;

  reporter::pointer _reporter;
  shared_io_context _io_context;
  const config _config;

  policy(const shared_io_context& io_context, const config& conf);
  void _start();

  void _from_script_child(const std::string& script_path,
                          const ConnectorMess& from_script_mess);

  void _on_script_child_end(const std::string& script_path, int pid);

 public:
  policy(policy const& p) = delete;
  policy& operator=(policy const& p) = delete;

  std::shared_ptr<policy> shared_from_this() {
    return std::static_pointer_cast<policy>(
        com::centreon::connector::policy_interface::shared_from_this());
  }

  static void create(const shared_io_context& io_context, const config& conf);

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

#endif
