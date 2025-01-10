/**
 * Copyright 2014-2015, 2021-2025 Centreon
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

#ifndef CCB_BAM_BA_HH
#define CCB_BAM_BA_HH

#include "bbdo/bam/ba_duration_event.hh"
#include "bbdo/bam/inherited_downtime.hh"
#include "com/centreon/broker/bam/computable.hh"
#include "com/centreon/broker/bam/configuration/ba.hh"
#include "com/centreon/broker/bam/impact_values.hh"
#include "com/centreon/broker/bam/service_listener.hh"

namespace com::centreon::broker::bam {
// Forward declaration.
class kpi;

/**
 *  @class ba ba.hh "com/centreon/broker/bam/ba.hh"
 *  @brief Business activity.
 *
 *  Represents a BA that gets computed every time an impact changes
 *  of value.
 */
class ba : public computable, public service_listener {
  const uint32_t _id;
  const configuration::ba::state_source _state_source;
  const uint32_t _host_id;
  const uint32_t _service_id;
  const bool _generate_virtual_status;
  bool _in_downtime;
  timestamp _last_kpi_update;
  std::unique_ptr<pb_inherited_downtime> _inherited_downtime;

  std::vector<std::shared_ptr<pb_ba_event>> _initial_events;

  void _open_new_event(io::stream* visitor,
                       com::centreon::broker::bam::state service_hard_state);
  void _compute_inherited_downtime(io::stream* visitor);
  void _commit_initial_events(io::stream* visitor);

 protected:
  int32_t _acknowledgement_count{0};

 public:
  struct impact_info {
    std::shared_ptr<kpi> kpi_ptr;
    impact_values hard_impact;
    impact_values soft_impact;
    bool in_downtime;
  };

 protected:
  std::shared_ptr<pb_ba_event> _event;
  std::string _name;
  constexpr static int _recompute_limit = 100;

  /* _level_critical and _level_warning are given by the configuration.
   * levels are used in:
   * * ba_impact: Here _level_critical <= _level_warning. Each kpi soustracts
   *   some points to the current level.
   * * ba_ratio_percent: Here _level_critical and _level_warning are numbers
   *   between 0 and 100. And _level_warning <= _level_critical
   * * ba_ratio_number: Here, it is almost the same as ratio_percent but we
   *   work with the number of kpis.
   **/
  double _level_critical{0.0};
  double _level_warning{0.0};

  /* _level_hard and _level_soft are the current levels of the ba, the soft one
   * and the hard one. */
  double _level_hard{100.0};
  double _level_soft{100.0};

  std::unordered_map<kpi*, impact_info> _impacts;
  bool _valid{true};
  configuration::ba::downtime_behaviour _dt_behaviour{
      configuration::ba::dt_ignore};
  int _recompute_count{0};

  static double _normalize(double d);
  virtual void _apply_impact(kpi* kpi_ptr, impact_info& impact) = 0;
  virtual void _unapply_impact(kpi* kpi_ptr, impact_info& impact) = 0;
  virtual bool _apply_changes(kpi* child,
                              const impact_values& new_hard_impact,
                              const impact_values& new_soft_impact,
                              bool in_downtime) = 0;
  virtual std::shared_ptr<pb_ba_status> _generate_ba_status(
      bool state_changed) const = 0;
  std::shared_ptr<io::data> _generate_virtual_service_status() const;

 public:
  ba(uint32_t id,
     uint32_t host_id,
     uint32_t service_id,
     configuration::ba::state_source source,
     bool generate_virtual_status,
     const std::shared_ptr<spdlog::logger>& logger);
  ba(const ba&) = delete;
  virtual ~ba() noexcept = default;
  ba& operator=(ba const& other) = delete;
  void add_impact(std::shared_ptr<kpi> const& impact);
  virtual double get_downtime_impact_hard() { return 0.0; }
  virtual double get_downtime_impact_soft() { return 0.0; }
  int32_t get_ack_impact_hard() const;
  std::shared_ptr<pb_ba_event> get_ba_event();
  uint32_t get_id() const;
  uint32_t get_host_id() const;
  uint32_t get_service_id() const;
  bool in_downtime() const;
  timestamp get_last_kpi_update() const;
  std::string const& get_name() const;
  virtual std::string get_output() const = 0;
  virtual std::string get_perfdata() const = 0;
  virtual state get_state_hard() const = 0;
  virtual state get_state_soft() const = 0;
  configuration::ba::state_source get_state_source() const;
  void remove_impact(std::shared_ptr<kpi> const& impact);
  void set_initial_event(const pb_ba_event& event);
  void set_name(std::string const& name);
  void set_valid(bool valid);
  void set_downtime_behaviour(configuration::ba::downtime_behaviour value);
  void set_state_source(configuration::ba::state_source source);
  void visit(io::stream* visitor);
  void service_update(std::shared_ptr<neb::downtime> const& dt,
                      io::stream* visitor) override;
  void service_update(std::shared_ptr<neb::pb_downtime> const& dt,
                      io::stream* visitor) override;
  void save_inherited_downtime(persistent_cache& cache) const;
  void set_inherited_downtime(inherited_downtime const& dwn);
  void set_inherited_downtime(pb_inherited_downtime const& dwn);
  void set_level_critical(double level);
  void set_level_warning(double level);
  void update_from(computable* child, io::stream* visitor) override;
  std::string object_info() const override;
  void dump(const std::string& filename) const;
  void dump(std::ofstream& output) const override;
};
}  // namespace com::centreon::broker::bam

#endif  // !CCB_BAM_BA_HH
