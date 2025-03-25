/**
 * Copyright 2014, 2021-2024 Centreon
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

#ifndef CCB_BAM_BOOL_SERVICE_HH
#define CCB_BAM_BOOL_SERVICE_HH

#include "com/centreon/broker/bam/bool_value.hh"
#include "com/centreon/broker/bam/service_listener.hh"

namespace com::centreon::broker::bam {
/**
 *  @class bool_service bool_service.hh
 * "com/centreon/broker/bam/bool_service.hh"
 *  @brief Evaluation of a service state.
 *
 *  This class compares the state of a service to compute a boolean
 *  value.
 */
class bool_service : public bool_value, public service_listener {
  const uint32_t _host_id;
  const uint32_t _service_id;
  short _state_hard;
  bool _state_known;
  bool _in_downtime;

 public:
  typedef std::shared_ptr<bool_service> ptr;

  bool_service(uint32_t host_id,
               uint32_t service_id,
               const std::shared_ptr<spdlog::logger>& logger);
  ~bool_service() noexcept = default;
  bool_service(const bool_service&) = delete;
  bool_service& operator=(const bool_service&) = delete;
  uint32_t get_host_id() const;
  uint32_t get_service_id() const;
  void service_update(const service_state& s) override;
  void service_update(const std::shared_ptr<neb::pb_service>& status,
                      io::stream* visitor = nullptr) override;
  void service_update(const std::shared_ptr<neb::pb_service_status>& status,
                      io::stream* visitor = nullptr) override;
  void service_update(
      const std::shared_ptr<neb::pb_adaptive_service_status>& status,
      io::stream* visitor = nullptr) override;
  void service_update(const std::shared_ptr<neb::service_status>& status,
                      io::stream* visitor = nullptr) override;
  double value_hard() const override;
  bool boolean_value() const override;
  bool state_known() const override;
  bool in_downtime() const override;
  void update_from(computable* child, io::stream* visitor) override;
  std::string object_info() const override;
  void dump(std::ofstream& output) const override;
};
}  // namespace com::centreon::broker::bam

#endif  // !CCB_BAM_BOOL_SERVICE_HH
