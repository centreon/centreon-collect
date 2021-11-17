/*
** Copyright 2014, 2021 Centreon
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

#ifndef CCB_BAM_KPI_BOOLEXP_HH
#define CCB_BAM_KPI_BOOLEXP_HH

#include <memory>

#include "bbdo/bam/state.hh"
#include "com/centreon/broker/bam/kpi.hh"
#include "com/centreon/broker/io/stream.hh"
#include "com/centreon/broker/namespace.hh"
#include "impact_values.hh"

CCB_BEGIN()

namespace bam {
// Forward declaration.
class bool_expression;
class computable;

/**
 *  @class kpi_boolexp kpi_boolexp.hh "com/centreon/broker/bam/kpi_boolexp.hh"
 *  @brief Boolean expression as a KPI.
 *
 *  This class allows you to use boolean expressions (class
 *  bool_expression) as a KPI for a BA.
 */
class kpi_boolexp : public kpi {
 private:
  state _get_state() const;
  std::shared_ptr<bool_expression> _boolexp;
  double _impact;

  void _fill_impact(impact_values& impact);
  void _open_new_event(io::stream* visitor, int impact, state state);

 public:
  kpi_boolexp(uint32_t kpi_id, uint32_t ba_id);
  ~kpi_boolexp() noexcept = default;
  kpi_boolexp(const kpi_boolexp&) = delete;
  kpi_boolexp& operator=(const kpi_boolexp&) = delete;
  bool child_has_update(computable* child, io::stream* visitor = nullptr);
  bool in_downtime() const;
  double get_impact() const;
  void impact_hard(impact_values& hard_impact);
  void impact_soft(impact_values& soft_impact);
  void link_boolexp(std::shared_ptr<bool_expression>& my_boolexp);
  void set_impact(double impact);
  void unlink_boolexp();
  void visit(io::stream* visitor);
  bool ok_state() const;
};
}  // namespace bam

CCB_END()

#endif  // !CCB_BAM_KPI_BOOLEXP_HH
