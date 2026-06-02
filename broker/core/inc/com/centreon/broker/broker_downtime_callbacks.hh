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

#ifndef CCB_BROKER_DOWNTIME_CALLBACKS_HH
#define CCB_BROKER_DOWNTIME_CALLBACKS_HH

#include <absl/container/btree_map.h>
#include <absl/container/flat_hash_map.h>
#include <absl/synchronization/mutex.h>
#include <boost/asio/steady_timer.hpp>
#include "com/centreon/broker/neb/internal.hh"
#include "common/downtimes/downtime.hh"
#include "common/downtimes/downtime_callbacks.hh"

namespace com::centreon::broker {

/**
 * @brief Broker-side implementation of downtime_callbacks.
 *
 * This class implements the integration contract required by the common
 * downtimes library (common/downtimes/downtime_callbacks.hh) for the Broker
 * process (cbd).
 *
 * See broker/doc/downtimes-integration-en.md for the full integration guide.
 */
class broker_downtime_callbacks
    : public com::centreon::common::downtimes::downtime_callbacks {
  mutable absl::Mutex _downtimes_m;
  /* Cache of published pb_downtime objects, keyed by downtime ID.
   * Mirrors cbmod's _downtimes: events are updated in place and re-published.
   */
  absl::flat_hash_map<uint64_t, std::shared_ptr<neb::pb_downtime>> _downtimes
      ABSL_GUARDED_BY(_downtimes_m);

  mutable absl::Mutex _pending_flex_m;
  /* Pending flexible downtime counters, keyed by {host_id, service_id}
   * (service_id == 0 for hosts). Not in BBDO — maintained in-process only. */
  absl::flat_hash_map<std::pair<uint64_t, uint64_t>, int32_t>
      _pending_flex_downtimes ABSL_GUARDED_BY(_pending_flex_m);

  /** Scheduled downtime events, keyed by fire time.
   *  A nullptr value is a sentinel for expire-downtime sweeps. */
  absl::btree_multimap<
      time_t,
      std::shared_ptr<com::centreon::common::downtimes::downtime>>
      _scheduled_downtimes;
  boost::asio::steady_timer _downtime_timer;

  void _arm_timer();
  void _on_timer();

 public:
  broker_downtime_callbacks(boost::asio::io_context& io_context);
  ~broker_downtime_callbacks() override = default;

  /* --- Object existence and naming --- */

  bool host_exists(uint64_t host_id) const override;
  bool service_exists(uint64_t host_id, uint64_t service_id) const override;
  bool resource_exists(uint64_t host_id, uint64_t service_id) const override;
  bool is_resource_ok(uint64_t host_id, uint64_t service_id) const override;
  std::string get_host_name(uint64_t host_id) const override;
  std::pair<std::string, std::string> get_host_and_service_names(
      uint64_t host_id,
      uint64_t service_id) const noexcept override;

  /* --- Comment management --- */

  uint64_t create_downtime_comment(uint64_t host_id,
                                   uint64_t service_id,
                                   const std::string& author,
                                   const std::string& comment_data) override;
  void delete_downtime_comment(uint64_t comment_id) override;

  /* --- Anomaly detection lookup --- */

  std::vector<uint64_t> get_anomaly_detection_services(
      uint64_t host_id,
      uint64_t service_id) const override;

  /* --- Event scheduling --- */

  void schedule_downtime_check(uint64_t downtime_id, time_t when) override;
  void schedule_expire_downtime(time_t when) override;
  void remove_downtime_check(uint64_t downtime_id) override;

  /* --- State mutations --- */

  bool inc_pending_flex_downtime(uint64_t host_id,
                                 uint64_t service_id) override;
  void start_downtime_effect(uint64_t host_id,
                             uint64_t service_id,
                             const std::string& author,
                             const std::string& comment) override;
  void end_downtime_effect(uint64_t host_id,
                           uint64_t service_id,
                           bool is_fixed,
                           bool incremented_pending,
                           const std::string& author,
                           const std::string& comment) override;
  bool cancel_downtime(uint64_t host_id,
                       uint64_t service_id,
                       bool is_fixed,
                       bool incremented_pending,
                       bool is_in_effect) override;

  /* --- Broker notification --- */

  void notify_broker(action act,
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
                     uint64_t downtime_id) override;
};

}  // namespace com::centreon::broker

#endif /* !CCB_BROKER_DOWNTIME_CALLBACKS_HH */
