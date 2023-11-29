/*
 * Copyright 2014, 2021-2023 Centreon
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

#ifndef CCB_BAM_KPI_BA_HH
#define CCB_BAM_KPI_BA_HH

#include "bbdo/bam/state.hh"
#include "com/centreon/broker/bam/impact_values.hh"
#include "com/centreon/broker/bam/internal.hh"
#include "com/centreon/broker/bam/kpi.hh"
#include "com/centreon/broker/io/stream.hh"
#include "com/centreon/broker/namespace.hh"

CCB_BEGIN()

namespace bam {
// Forward declaration.
class ba;
class computable;

/**
 *  @class kpi_ba kpi_ba.hh "com/centreon/broker/bam/kpi_ba.hh"
 *  @brief BA as a KPI.
 *
 *  This class allows you to use a BA (class ba) as a KPI for another
 *  BA.
 */
class kpi_ba : public kpi {
 private:
  std::shared_ptr<ba> _ba;
  double _impact_critical;
  double _impact_warning;
  double _impact_unknown;

  void _fill_impact(impact_values& impact,
                    state state,
                    double acknowledgement,
                    double downtime);
  void _open_new_event(io::stream* visitor,
                       int impact,
                       com::centreon::broker::State ba_state,
                       const timestamp& event_start_time);

 public:
  kpi_ba(uint32_t kpi_id, uint32_t ba_id, const std::string& ba_name);
  ~kpi_ba() noexcept = default;
  kpi_ba(const kpi_ba&) = delete;
  kpi_ba& operator=(const kpi_ba&) = delete;
  double get_impact_critical() const;
  double get_impact_warning() const;
  void impact_hard(impact_values& hard_impact) override;
  void impact_soft(impact_values& soft_impact) override;
  void link_ba(std::shared_ptr<ba>& my_ba);
  void set_impact_critical(double impact);
  void set_impact_warning(double impact);
  void set_impact_unknown(double impact);
  void unlink_ba();
  void visit(io::stream* visitor) override;
  bool ok_state() const override;
  bool in_downtime() const override;
  virtual void update_from(computable* child, io::stream* visitor) override;
  std::string object_info() const override;
  void dump(std::ofstream& output) const override;
};
}  // namespace bam

CCB_END()

#endif  // !CCB_BAM_KPI_BA_HH
