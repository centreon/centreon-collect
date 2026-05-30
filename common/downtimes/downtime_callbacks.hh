/**
 * Copyright 2026 Centreon (https://www.centreon.com/)
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * For more information : contact@centreon.com
 *
 */

#ifndef CCC_DOWNTIMES_DOWNTIME_CALLBACKS_HH
#define CCC_DOWNTIMES_DOWNTIME_CALLBACKS_HH

namespace com::centreon::common::downtimes {
class downtime_callbacks {
 public:
  enum action { ADD, START, DELETE, LOAD, STOP };
  enum attribute { ATTR_NONE = 0, ATTR_STOP_NORMAL = 1, ATTR_STOP_CANCELLED = 2 };

  virtual ~downtime_callbacks() = default;

  virtual bool host_exists(uint64_t host_id) const = 0;
  virtual bool service_exists(uint64_t host_id, uint64_t service_id) const = 0;
  virtual std::string get_host_name(uint64_t host_id) const = 0;
  virtual std::pair<std::string, std::string> get_host_and_service_names(
      uint64_t host_id,
      uint64_t service_id) const noexcept = 0;
  virtual std::vector<uint64_t> get_anomaly_detection_services(
      uint64_t host_id,
      uint64_t service_id) const = 0;
  virtual bool cancel_downtime(uint64_t host_id,
                               uint64_t service_id,
                               bool is_fixed,
                               bool incremented_pending,
                               bool is_in_effect) = 0;
  virtual void schedule_downtime_check(uint64_t downtime_id,
                                       time_t when) = 0;
  virtual void schedule_expire_downtime(time_t when) = 0;
  virtual void remove_downtime_check(uint64_t downtime_id) = 0;

  virtual bool object_exists(uint64_t host_id, uint64_t service_id) const = 0;
  virtual bool is_object_ok(uint64_t host_id, uint64_t service_id) const = 0;
  virtual bool inc_pending_flex_downtime(uint64_t host_id,
                                         uint64_t service_id) = 0;
  virtual void start_downtime_effect(uint64_t host_id,
                                     uint64_t service_id,
                                     const std::string& author,
                                     const std::string& comment) = 0;
  virtual void end_downtime_effect(uint64_t host_id,
                                   uint64_t service_id,
                                   bool is_fixed,
                                   bool incremented_pending,
                                   const std::string& author,
                                   const std::string& comment) = 0;

  virtual void notify_broker(action act,
                             attribute attr,
                             uint64_t host_id,
                             uint64_t service_id,
                             const std::string& author,
                             const std::string& comment,
                             time_t entry_time,
                             time_t start_time,
                             time_t end_time,
                             bool fixed,
                             uint64_t triggered_by,
                             uint32_t duration,
                             uint64_t downtime_id) const = 0;
};

}  // namespace com::centreon::common::downtimes

#endif /* !CCC_DOWNTIMES_DOWNTIME_CALLBACKS_HH */
