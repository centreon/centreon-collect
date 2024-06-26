/**
 * Copyright 2014, 2022-2024 Centreon
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

#ifndef CCB_BAM_CONFIGURATION_KPI_HH
#define CCB_BAM_CONFIGURATION_KPI_HH

#include "com/centreon/broker/bam/internal.hh"

namespace com::centreon::broker {

namespace bam {
namespace configuration {
/**
 *  @class kpi kpi.hh "com/centreon/broker/bam/configuration/kpi.hh"
 *  @brief   Abstraction for representing a business interest in the
 *           form of a percentage value.
 *
 *  KPI configuration. It holds the rule of the KPI such as its
 *  impact, which service/BA it targets, ...
 */
class kpi {
  uint32_t _id;
  int16_t _state_type;
  uint32_t _host_id;
  uint32_t _service_id;
  uint32_t _ba_id;
  uint32_t _indicator_ba_id;
  uint32_t _meta_id;
  uint32_t _boolexp_id;
  short _status;
  bool _downtimed;
  bool _acknowledged;
  bool _ignore_downtime;
  bool _ignore_acknowledgement;
  double _impact_warning;
  double _impact_critical;
  double _impact_unknown;
  KpiEvent _event;
  std::string _name;

 public:
  kpi(uint32_t id = 0,
      short state_type = 0,
      uint32_t host_id = 0,
      uint32_t service_id = 0,
      uint32_t ba_id = 0,
      uint32_t indicator_ba = 0,
      uint32_t meta_id = 0,
      uint32_t boolexp_id = 0,
      short status = 0,
      bool downtimed = false,
      bool acknowledged = false,
      bool ignoredowntime = false,
      bool ignoreacknowledgement = false,
      double warning = 0,
      double critical = 0,
      double unknown = 0,
      const std::string& name = "");

  bool operator==(kpi const& other) const;
  bool operator!=(kpi const& other) const;

  uint32_t get_id() const;
  short get_state_type() const;
  uint32_t get_host_id() const;
  uint32_t get_service_id() const;
  bool is_service() const;
  bool is_ba() const;
  bool is_meta() const;
  bool is_boolexp() const;
  uint32_t get_ba_id() const;
  uint32_t get_indicator_ba_id() const;
  uint32_t get_meta_id() const;
  uint32_t get_boolexp_id() const;
  short get_status() const;
  bool is_downtimed() const;
  bool is_acknowledged() const;
  bool ignore_downtime() const;
  bool ignore_acknowledgement() const;
  double get_impact_warning() const;
  double get_impact_critical() const;
  double get_impact_unknown() const;
  const KpiEvent& get_opened_event() const;
  const std::string get_name() const { return _name; }

  void set_id(uint32_t id);
  void set_state_type(short state_type);
  void set_host_id(uint32_t host_id);
  void set_service_id(uint32_t service_id);
  void set_ba_id(uint32_t ba_id);
  void set_indicator_ba_id(uint32_t ba_id);
  void set_meta_id(uint32_t meta_id);
  void set_boolexp_id(uint32_t boolexp_id);
  void set_status(short status);
  void set_downtimed(bool downtimed);
  void set_acknowledged(bool acknowledged);
  void ignore_downtime(bool ignore);
  void ignore_acknowledgement(bool ignore);
  void set_impact_warning(double impact);
  void set_impact_critical(double impact);
  void set_impact_unknown(double impact);
  void set_opened_event(const KpiEvent& kpi_event);
};
}  // namespace configuration
}  // namespace bam

}  // namespace com::centreon::broker

#endif  // CCB_BAM_CONFIGURATION_KPI_HH
