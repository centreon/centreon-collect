/**
 * Copyright 2014-2024 Centreon
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

#include "com/centreon/broker/bam/configuration/applier/state.hh"

#include <algorithm>
#include <fmt/ranges.h>

#include "com/centreon/broker/bam/internal.hh"

#include "broker/core/config/applier/state.hh"
#include "com/centreon/broker/bam/exp_builder.hh"
#include "com/centreon/broker/neb/bbdo2_to_bbdo3.hh"

using namespace com::centreon::exceptions;
using namespace com::centreon::broker;
using namespace com::centreon::broker::bam::configuration;

/**
 *  Get BA identifier for circular path search.
 *
 *  @return BA identifier for circular path search.
 */
static std::string ba_node_id(uint32_t ba_id) {
  return fmt::format("BA {}", ba_id);
}

/**
 *  Get boolean expression identifier for circular path search.
 *
 *  @return Boolean expression identifier for circular path search.
 */
static std::string boolexp_node_id(uint32_t boolexp_id) {
  return fmt::format("boolean expression {}", boolexp_id);
}

/**
 *  Get KPI identifier for circular path search.
 *
 *  @return KPI identifier for circular path search.
 */
static std::string kpi_node_id(uint32_t kpi_id) {
  return fmt::format("KPI {}", kpi_id);
}

/**
 *  Get meta-service identifier for circular path search.
 *
 *  @return Meta-service identifier for circular path search.
 */
static std::string meta_node_id(uint32_t meta_id) {
  return fmt::format("meta-service {}", meta_id);
}

/**
 *  Get service identifier for circular path search.
 *
 *  @return Service identifier for circular path search.
 */
static std::string service_node_id(uint64_t host_id, uint64_t service_id) {
  return fmt::format("service ({}, {})", host_id, service_id);
}

/**
 * @brief Constructor
 *
 * @param logger The logger to use with this class.
 */
applier::state::state(const std::shared_ptr<spdlog::logger>& logger)
    : _logger{logger},
      _ba_applier(_logger),
      _book_service(_logger),
      _kpi_applier(_logger),
      _bool_exp_applier(_logger) {}

/**
 *  Apply configuration.
 *
 *  @param[in] my_state  Configuration state.
 */
void applier::state::apply(bam::configuration::state const& my_state) {
  /* Search for circular paths in the object graph. A BA that is its own
   * ancestor is a configuration error, and a configuration error must cost
   * what is wrong and nothing else: this used to throw, which took the whole
   * BAM configuration down and left the failover reconnecting for ever. The
   * KPIs of a cycle are left out instead, which breaks it, and the BAs of the
   * cycle are applied invalid, UNKNOWN with the reason as output -- the
   * output lands in the comment column of mod_bam, where the user sees it. */
  circular_report cycles = _circular_check(my_state);

  // Really apply objects.
  _ba_applier.apply(my_state.get_bas(), _book_service);
  _bool_exp_applier.apply(my_state.get_bool_exps(),
                          my_state.get_hst_svc_mapping(), _book_service);
  if (cycles.kpis.empty())
    _kpi_applier.apply(my_state.get_kpis(), my_state.get_hst_svc_mapping(),
                       _ba_applier, _bool_exp_applier, _book_service);
  else {
    configuration::state::kpis kpis(my_state.get_kpis());
    for (uint32_t kpi_id : cycles.kpis)
      kpis.erase(kpi_id);
    _kpi_applier.apply(kpis, my_state.get_hst_svc_mapping(), _ba_applier,
                       _bool_exp_applier, _book_service);
  }

  /* After the BA applier, which sets every BA valid again on each apply. */
  for (uint32_t ba_id : cycles.bas) {
    std::shared_ptr<bam::ba> b = _ba_applier.find_ba(ba_id);
    if (!b)
      continue;
    std::string reason = fmt::format(
        "Circular definition detected. BA {} includes itself as a KPI.",
        b->get_name());
    _logger->error("BAM: {}", reason);
    b->set_valid(false, reason);
  }
}

/**
 *  Get the book of service listeners.
 *
 *  @return Book of service listeners.
 */
bam::service_book& applier::state::book_service() {
  return _book_service;
}

/**
 *  @brief Visit applied state.
 *
 *  This method is used to generate default status.
 *
 *  @param[out] visitor  Visitor.
 */
void applier::state::visit(io::stream* visitor, bool seed_service_status) {
  _ba_applier.visit(visitor, seed_service_status);
  _kpi_applier.visit(visitor);
}

/**
 *  Circular check node constructor.
 */
applier::state::circular_check_node::circular_check_node()
    : in_visit(false), visited(false), what(other), id(0) {}

/**
 *  Check BA computation graph for circular paths.
 *
 *  @param[in] my_state  Configuration state.
 */
applier::state::circular_report applier::state::_circular_check(
    configuration::state const& my_state) {
  // In this method, nodes are referenced by an internal ID named after
  // object type and ID.

  //
  // Populate graph with all objects.
  //
  _nodes.clear();
  _path.clear();

  // Add BAs.
  for (configuration::state::bas::const_iterator it(my_state.get_bas().begin()),
       end(my_state.get_bas().end());
       it != end; ++it) {
    circular_check_node& n(_nodes[ba_node_id(it->first)]);
    n.what = circular_check_node::ba;
    n.id = it->first;
    n.targets.insert(
        service_node_id(it->second.get_host_id(), it->second.get_service_id()));
  }

  // Add boolean expressions.
  for (configuration::state::bool_exps::const_iterator
           it(my_state.get_bool_exps().begin()),
       end(my_state.get_bool_exps().end());
       it != end; ++it) {
    std::string bool_id(boolexp_node_id(it->first));
    _nodes[bool_id];
    try {
      exp_parser parsr(it->second.get_expression());
      exp_builder buildr(parsr.get_postfix(), my_state.get_hst_svc_mapping(),
                         _logger);
      for (std::list<bool_service::ptr>::const_iterator
               it_svc(buildr.get_services().begin()),
           end_svc(buildr.get_services().end());
           it_svc != end_svc; ++it_svc)
        _nodes[service_node_id((*it_svc)->get_host_id(),
                               (*it_svc)->get_service_id())]
            .targets.insert(bool_id);
    }
    // Silently ignore parsing errors.
    catch (std::exception const& e) {
      (void)e;
    }
  }
  // Add KPIs.
  for (configuration::state::kpis::const_iterator
           it(my_state.get_kpis().begin()),
       end(my_state.get_kpis().end());
       it != end; ++it) {
    std::string kpi_id(kpi_node_id(it->first));
    circular_check_node& n(_nodes[kpi_id]);
    n.what = circular_check_node::kpi;
    n.id = it->first;
    n.targets.insert(ba_node_id(it->second.get_ba_id()));
    std::string node_id;
    if (it->second.is_ba())
      node_id = ba_node_id(it->second.get_indicator_ba_id());
    else if (it->second.is_meta())
      node_id = meta_node_id(it->second.get_meta_id());
    else if (it->second.is_boolexp())
      node_id = boolexp_node_id(it->second.get_boolexp_id());
    else if (it->second.is_service())
      node_id = service_node_id(it->second.get_host_id(),
                                it->second.get_service_id());
    else
      continue;
    _nodes[node_id].targets.insert(kpi_id);
  }

  // Process all nodes.
  circular_report report;
  for (auto& [name, node] : _nodes)
    if (!node.visited)
      _circular_check(name, node, report);
  _nodes.clear();
  return report;
}

/**
 *  Check a node for circular path.
 *
 *  A depth-first walk: a node met again while still on the path closes a
 *  cycle, which is the path from that node to here. Its KPIs and BAs are
 *  added to the report and the walk goes on, so that every cycle of the
 *  graph is found in one pass.
 *
 *  @param[in]     name    The node identifier.
 *  @param[in,out] n       Target node.
 *  @param[out]    report  Where the cycles are collected.
 */
void applier::state::_circular_check(const std::string& name,
                                     applier::state::circular_check_node& n,
                                     circular_report& report) {
  if (n.in_visit) {
    auto from = std::find(_path.begin(), _path.end(), name);
    _logger->error("BAM: circular definition: {} -> {}",
                   fmt::join(from, _path.end(), " -> "), name);
    for (; from != _path.end(); ++from) {
      const circular_check_node& m = _nodes.find(*from)->second;
      if (m.what == circular_check_node::kpi)
        report.kpis.insert(m.id);
      else if (m.what == circular_check_node::ba)
        report.bas.insert(m.id);
    }
    return;
  }
  if (n.visited)
    return;
  n.in_visit = true;
  _path.push_back(name);
  for (const std::string& target : n.targets) {
    auto it_node = _nodes.find(target);
    if (it_node != _nodes.end())
      _circular_check(target, it_node->second, report);
  }
  _path.pop_back();
  n.visited = true;
  n.in_visit = false;
}

/**
 *  Save the state to the cache.
 *
 *  @param[in] cache  The cache.
 */
void applier::state::save_to_cache(
    const std::string& name,
    const std::deque<std::string>& pending_ext_cmds) {
  ServicesBookState cache;
  _logger->trace("BAM: Saving states to cache");
  _book_service.save_to_cache(&cache);
  for (auto& cmd : pending_ext_cmds)
    cache.add_pending_external_commands(cmd);

  auto& state = config::applier::state::instance();
  std::filesystem::path cache_file =
      fmt::format("{}.cache.{}", state.cache_dir(), name);

  std::ofstream ofs(cache_file, std::ios::binary | std::ios::trunc);
  if (ofs) {
    if (!cache.SerializeToOstream(&ofs)) {
      _logger->error("BAM: could not serialize BAM states to cache file {}",
                     cache_file.string());
    } else {
      _logger->debug("BAM: BAM states saved to cache file {}",
                     cache_file.string());
    }
    ofs.close();
  } else {
    _logger->error("BAM: could not open BAM cache file '{}' for writing",
                   cache_file.string());
  }
  _logger->trace("BAM: States correctly saved");
}

/**
 *  Load the state from the cache.
 *
 *  @param[in] cache  the cache.
 */
void applier::state::load_from_cache(
    const std::string& name,
    std::deque<std::string>& pending_ext_cmds) {
  _logger->debug(
      "BAM: restoring virtual service states and pending external commands "
      "from cache");

  std::ifstream ifs;
  auto& state = config::applier::state::instance();
  std::filesystem::path cache_file =
      fmt::format("{}.cache.{}", state.cache_dir(), name);
  ifs.open(cache_file, std::ios::binary);
  if (!ifs) {
    _logger->debug("BAM: could not open BAM cache file '{}' for reading",
                   cache_file.string());
    return;
  }
  ServicesBookState cache;
  if (!cache.ParseFromIstream(&ifs)) {
    _logger->error("BAM: could not parse BAM states from cache file {}",
                   cache_file.string());
    return;
  }
  ifs.close();
  _book_service.apply(cache);
  for (auto& cmd : cache.pending_external_commands()) {
    pending_ext_cmds.push_back(cmd);
  }
  _logger->debug(
      "BAM: virtual service states restored from cache file {} (inherited "
      "downtimes are not cached; they are recomputed from KPIs/DB)",
      cache_file.string());
}

/**
 * @brief Find in the applier a BA from its ID.
 *
 * @param id The ID of the BA
 *
 * @return A shared pointer to the BA or an empty shared pointer.
 */
std::shared_ptr<bam::ba> applier::state::find_ba(uint32_t id) const {
  return _ba_applier.find_ba(id);
}

/**
 *  Rebuild the inherited downtimes of the BAs after a restart.
 *
 *  See bam::ba::restore_inherited_downtime().
 */
void applier::state::restore_inherited_downtimes() {
  _ba_applier.restore_inherited_downtimes();
}
