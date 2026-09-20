/**
 * Copyright 2014-2015 Centreon
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

#ifndef CCB_BAM_CONFIGURATION_APPLIER_STATE_HH
#define CCB_BAM_CONFIGURATION_APPLIER_STATE_HH

#include <absl/container/btree_set.h>
#include <absl/container/flat_hash_set.h>

#include "com/centreon/broker/bam/configuration/applier/ba.hh"
#include "com/centreon/broker/bam/configuration/applier/bool_expression.hh"
#include "com/centreon/broker/bam/configuration/applier/kpi.hh"
#include "com/centreon/broker/bam/service_book.hh"

namespace com::centreon::broker::bam {
// Forward declaration.
class monitoring_stream;

namespace configuration {
// Forward declaration.
class state;

namespace applier {
/**
 *  @class state state.hh
 * "com/centreon/broker/bam/configuration/applier/state.hh"
 *  @brief Apply global state of the BAM engine.
 *
 *  Take the configuration of the BAM engine and apply it.
 */
class state {
  std::shared_ptr<spdlog::logger> _logger;

  struct circular_check_node {
    enum kind { other, ba, kpi };
    circular_check_node();

    bool in_visit;
    bool visited;
    /* What the node stands for, so that a cycle can be turned into the KPIs
     * to drop and the BAs to invalidate. */
    kind what;
    uint32_t id;
    std::set<std::string> targets;
  };
  /* What a run of _circular_check found: the KPIs that take part in a cycle
   * and the BAs that do. Dropping the former breaks every cycle whatever the
   * order the graph was walked in; the latter are reported as invalid. */
  struct circular_report {
    absl::flat_hash_set<uint32_t> kpis;
    absl::btree_set<uint32_t> bas;
  };
  ba _ba_applier;
  service_book _book_service;
  kpi _kpi_applier;
  bool_expression _bool_exp_applier;
  std::unordered_map<std::string, circular_check_node> _nodes;
  /* The nodes on the current path of the depth-first walk. */
  std::vector<std::string> _path;

  circular_report _circular_check(configuration::state const& my_state);
  void _circular_check(const std::string& name,
                       circular_check_node& n,
                       circular_report& report);

 public:
  state(const std::shared_ptr<spdlog::logger>& logger);
  ~state() noexcept = default;
  state(const state&) = delete;
  state& operator=(state const& other) = delete;
  void apply(configuration::state const& my_state);
  service_book& book_service();
  void visit(io::stream* visitor, bool seed_service_status);
  void save_to_cache(const std::string& name,
                     const std::deque<std::string>& pending_ext_cmds);
  void load_from_cache(const std::string& name,
                       std::deque<std::string>& pending_ext_cmds);
  std::shared_ptr<bam::ba> find_ba(uint32_t id) const;
  void restore_inherited_downtimes();
};
}  // namespace applier
}  // namespace configuration
}  // namespace com::centreon::broker::bam

#endif  // !CCB_BAM_CONFIGURATION_APPLIER_STATE_HH
