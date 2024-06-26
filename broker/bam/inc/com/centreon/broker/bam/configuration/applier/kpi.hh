/**
 * Copyright 2014-2015, 2022-2024 Centreon
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

#ifndef CCB_BAM_CONFIGURATION_APPLIER_KPI_HH
#define CCB_BAM_CONFIGURATION_APPLIER_KPI_HH

#include "com/centreon/broker/bam/configuration/kpi.hh"
#include "com/centreon/broker/bam/configuration/state.hh"
#include "com/centreon/broker/bam/kpi.hh"
#include "com/centreon/broker/io/stream.hh"

namespace com::centreon::broker {

namespace bam {
// Forward declaration.
class service_book;

namespace configuration {
namespace applier {
// Forward declarations.
class ba;
class bool_expression;

/**
 *  @class kpi kpi.hh "com/centreon/broker/bam/configuration/applier/kpi.hh"
 *  @brief Apply KPI configuration.
 *
 *  Take the configuration of KPIs and apply it.
 */
class kpi {
  std::shared_ptr<spdlog::logger> _logger;
  struct applied {
    configuration::kpi cfg;
    std::shared_ptr<bam::kpi> obj;
  };
  std::map<uint32_t, applied> _applied;
  ba* _bas;
  service_book* _book;
  bool_expression* _boolexps;
  hst_svc_mapping const* _mapping;

  void _internal_copy(kpi const& other);
  std::shared_ptr<bam::kpi> _new_kpi(configuration::kpi const& cfg);
  void _invalidate_ba(configuration::kpi const& cfg);
  std::map<uint32_t, applied>::iterator _remove_kpi(
      std::map<uint32_t, applied>::iterator& kpi_it);
  void _resolve_kpi(configuration::kpi const& cfg,
                    const std::shared_ptr<bam::kpi>&);

 public:
  kpi(const std::shared_ptr<spdlog::logger>& logger);
  kpi(kpi const& other);
  ~kpi() noexcept = default;
  kpi& operator=(kpi const& other);
  void apply(configuration::state::kpis const& my_kpis,
             hst_svc_mapping const& mapping,
             ba& my_bas,
             bool_expression& my_boolexps,
             service_book& book);
  void visit(io::stream* visitor);
};
}  // namespace applier
}  // namespace configuration
}  // namespace bam

}  // namespace com::centreon::broker

#endif  // !CCB_BAM_CONFIGURATION_APPLIER_KPI_HH
