/*
 * Copyright 2014-2015, 2021-2023 Centreon
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

#ifndef CCB_BAM_KPI_SERVICE_HH
#define CCB_BAM_KPI_SERVICE_HH

#include <absl/container/flat_hash_set.h>
#include "bbdo/bam/state.hh"
#include "com/centreon/broker/bam/impact_values.hh"
#include "com/centreon/broker/bam/kpi.hh"
#include "com/centreon/broker/bam/service_listener.hh"
#include "com/centreon/broker/io/stream.hh"
#include "com/centreon/broker/timestamp.hh"

namespace com::centreon::broker::bam {
/**
 *  @class kpi_service kpi_service.hh "com/centreon/broker/bam/kpi_service.hh"
 *  @brief Service as a KPI.
 *
 *  Allows use of a service as a KPI that can impact a BA.
 */
class kpi_service : public service_listener, public kpi {
  const uint32_t _host_id;
  const uint32_t _service_id;

 private:
  void _fill_impact(impact_values& impact, state state);
  void _internal_copy(kpi_service const& right);
  void _open_new_event(io::stream* visitor, impact_values const& impacts);

  bool _acknowledged;
  bool _downtimed;
  absl::flat_hash_set<uint32_t> _downtime_ids;
  std::array<double, 5> _impacts;
  timestamp _last_check;
  std::string _output;
  std::string _perfdata;
  state _state_hard;
  state _state_soft;
  short _state_type;

 public:
  kpi_service(uint32_t kpi_id,
              uint32_t ba_id,
              uint32_t host_id,
              uint32_t service_id,
              const std::shared_ptr<spdlog::logger>& logger);
  ~kpi_service() noexcept = default;
  kpi_service(const kpi_service&) = delete;
  kpi_service& operator=(const kpi_service&) = delete;
  uint32_t get_host_id() const;
  double get_impact_critical() const;
  double get_impact_unknown() const;
  double get_impact_warning() const;
  uint32_t get_service_id() const;
  state get_state_hard() const;
  state get_state_soft() const;
  short get_state_type() const;
  void impact_hard(impact_values& impact) override;
  void impact_soft(impact_values& impact) override;
  bool in_downtime() const override;
  bool is_acknowledged() const;
  void service_update(std::shared_ptr<neb::service_status> const& status,
                      io::stream* visitor,
                      const std::shared_ptr<spdlog::logger>& logger) override;
  void service_update(const std::shared_ptr<neb::pb_service>& status,
                      io::stream* visitor,
                      const std::shared_ptr<spdlog::logger>& logger) override;
  void service_update(const std::shared_ptr<neb::pb_service_status>& status,
                      io::stream* visitor,
                      const std::shared_ptr<spdlog::logger>& logger) override;
  void service_update(const std::shared_ptr<neb::pb_acknowledgement>& ack,
                      io::stream* visitor,
                      const std::shared_ptr<spdlog::logger>& logger) override;
  void service_update(std::shared_ptr<neb::acknowledgement> const& ack,
                      io::stream* visitor,
                      const std::shared_ptr<spdlog::logger>& logger) override;
  void service_update(std::shared_ptr<neb::downtime> const& dt,
                      io::stream* visitor,
                      const std::shared_ptr<spdlog::logger>& logger) override;
  void service_update(const std::shared_ptr<neb::pb_downtime>& dt,
                      io::stream* visitor,
                      const std::shared_ptr<spdlog::logger>& logger) override;
  void set_acknowledged(bool acknowledged);
  void set_downtimed(bool downtimed);
  void set_impact_critical(double impact);
  void set_impact_unknown(double impact);
  void set_impact_warning(double impact);
  void set_state_hard(state state);
  void set_state_soft(state state);
  void set_state_type(short type);
  void visit(io::stream* visitor) override;
  virtual void set_initial_event(const KpiEvent& e) override;
  bool ok_state() const override;
  void update_from(computable* child,
                   io::stream* visitor,
                   const std::shared_ptr<spdlog::logger>& logger) override;
  std::string object_info() const override;
  void dump(std::ofstream& output) const;
};
}  // namespace com::centreon::broker::bam

#endif  // !CCB_BAM_KPI_SERVICE_HH
