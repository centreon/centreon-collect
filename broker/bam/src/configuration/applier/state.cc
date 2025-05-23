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

#include "com/centreon/broker/bam/internal.hh"

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
static std::string service_node_id(uint32_t host_id, uint32_t service_id) {
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
  // Search for circular path in object graph.
  _circular_check(my_state);

  // Really apply objects.
  _ba_applier.apply(my_state.get_bas(), _book_service);
  _bool_exp_applier.apply(my_state.get_bool_exps(),
                          my_state.get_hst_svc_mapping(), _book_service);
  _kpi_applier.apply(my_state.get_kpis(), my_state.get_hst_svc_mapping(),
                     _ba_applier, _bool_exp_applier, _book_service);
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
void applier::state::visit(io::stream* visitor) {
  _ba_applier.visit(visitor);
  _kpi_applier.visit(visitor);
}

/**
 *  Circular check node constructor.
 */
applier::state::circular_check_node::circular_check_node()
    : in_visit(false), visited(false) {}

/**
 *  Check BA computation graph for circular paths.
 *
 *  @param[in] my_state  Configuration state.
 */
void applier::state::_circular_check(configuration::state const& my_state) {
  // In this method, nodes are referenced by an internal ID named after
  // object type and ID.

  //
  // Populate graph with all objects.
  //
  _nodes.clear();

  // Add BAs.
  for (configuration::state::bas::const_iterator it(my_state.get_bas().begin()),
       end(my_state.get_bas().end());
       it != end; ++it) {
    circular_check_node& n(_nodes[ba_node_id(it->first)]);
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
  for (std::unordered_map<std::string, circular_check_node>::iterator
           it(_nodes.begin()),
       end(_nodes.end());
       it != end; ++it)
    if (!it->second.visited)
      _circular_check(it->second);
  _nodes.clear();
}

/**
 *  Check a node for circular path.
 *
 *  @param[in,out] n      Target node.
 */
void applier::state::_circular_check(applier::state::circular_check_node& n) {
  if (n.in_visit)
    throw msg_fmt("BAM: loop found in BA graph");
  if (!n.visited) {
    n.in_visit = true;
    for (std::set<std::string>::const_iterator it(n.targets.begin()),
         end(n.targets.end());
         it != end; ++it) {
      std::unordered_map<std::string, circular_check_node>::iterator it_node(
          _nodes.find(*it));
      if (it_node != _nodes.end())
        _circular_check(it_node->second);
    }
    n.visited = true;
    n.in_visit = false;
  }
}

/**
 *  Save the state to the cache.
 *
 *  @param[in] cache  The cache.
 */
void applier::state::save_to_cache(persistent_cache& cache) {
  _logger->trace("BAM: Saving states to cache");
  cache.transaction();
  _book_service.save_to_cache(cache);
  _ba_applier.save_to_cache(cache);
  cache.commit();
  _logger->trace("BAM: States correctly saved");
}

/**
 *  Load the state from the cache.
 *
 *  @param[in] cache  the cache.
 */
void applier::state::load_from_cache(persistent_cache& cache) {
  _logger->debug("BAM: Loading restoring inherited downtimes and BA states");

  std::shared_ptr<io::data> d;
  cache.get(d);
  uint32_t count_idt = 0;
  uint32_t count_ba = 0;
  while (d) {
    switch (d->type()) {
      case inherited_downtime::static_type():
        _ba_applier.apply_inherited_downtime(
            *std::static_pointer_cast<const pb_inherited_downtime>(
                neb::bbdo2_to_bbdo3(d)));
        count_idt++;
        break;
      case pb_inherited_downtime::static_type(): {
        const pb_inherited_downtime& dwn =
            *std::static_pointer_cast<const pb_inherited_downtime>(d);
        _ba_applier.apply_inherited_downtime(dwn);
        count_idt++;
      } break;
      case pb_services_book_state::static_type(): {
        const ServicesBookState& state =
            std::static_pointer_cast<const pb_services_book_state>(d)->obj();
        _book_service.apply_services_state(state);
        count_ba++;
      } break;
    }
    cache.get(d);
  }
  _logger->debug("BAM: {} Inherited downtimes and {} BA states restored",
                 count_idt, count_ba);
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
