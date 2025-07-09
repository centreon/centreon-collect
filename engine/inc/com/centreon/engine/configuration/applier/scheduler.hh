/**
 * Copyright 2011-2016 Centreon
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
#ifndef CCE_CONFIGURATION_APPLIER_SCHEDULER_HH
#define CCE_CONFIGURATION_APPLIER_SCHEDULER_HH

#include "com/centreon/engine/configuration/applier/difference.hh"
#include "com/centreon/engine/configuration/applier/pb_difference.hh"
#include "com/centreon/engine/exceptions/error.hh"
#include "common/engine_conf/state.pb.h"

// Forward declaration.
namespace com::centreon::engine {
class host;
class service;
class timed_event;

namespace configuration::applier {
/**
 *  @class scheduler scheduler.hh
 *  @brief Simple configuration applier for scheduler class.
 *
 *  Simple configuration applier for scheduler class.
 */
class scheduler {
 public:
  void apply(configuration::State& config,
             const pb_difference<configuration::Host, uint64_t>& diff_hosts,
             const pb_difference<configuration::Service,
                                 std::pair<uint64_t, uint64_t> >& diff_services,
             const pb_difference<configuration::Anomalydetection,
                                 std::pair<uint64_t, uint64_t> >&
                 diff_anomalydetections);
  static scheduler& instance();
  void clear();
  void remove_host(uint64_t host_id);
  void remove_service(uint64_t host_id, uint64_t service_id);

 private:
  scheduler();
  scheduler(scheduler const&) = delete;
  ~scheduler() noexcept;
  scheduler& operator=(scheduler const&) = delete;
  void _apply_misc_event();
  void _calculate_host_inter_check_delay(
      const configuration::InterCheckDelay& method);
  void _calculate_host_scheduling_params();
  void _calculate_service_inter_check_delay(
      const configuration::InterCheckDelay& method);
  void _calculate_service_interleave_factor(
      const configuration::InterleaveFactor& method);
  void _calculate_service_scheduling_params();
  timed_event* _create_misc_event(int type,
                                  time_t start,
                                  unsigned long interval,
                                  void* data = nullptr);
  std::vector<com::centreon::engine::host*> _get_hosts(
      const std::vector<uint64_t>& hst_ids,
      bool throw_if_not_found);
  std::vector<com::centreon::engine::service*> _get_anomalydetections(
      const std::vector<std::pair<uint64_t, uint64_t> >& ad_ids,
      bool throw_if_not_found);
  std::vector<com::centreon::engine::service*> _get_services(
      const std::vector<std::pair<uint64_t, uint64_t> >& ad_ids,
      bool throw_if_not_found);

  void _remove_misc_event(timed_event*& evt);
  void _schedule_host_events(
      std::vector<com::centreon::engine::host*> const& hosts);
  void _schedule_service_events(std::vector<engine::service*> const& services);
  void _unschedule_host_events(
      std::vector<com::centreon::engine::host*> const& hosts);
  void _unschedule_service_events(
      std::vector<engine::service*> const& services);

  configuration::State* _pb_config;
  timed_event* _evt_check_reaper;
  timed_event* _evt_command_check;
  timed_event* _evt_hfreshness_check;
  timed_event* _evt_orphan_check;
  timed_event* _evt_reschedule_checks;
  timed_event* _evt_retention_save;
  timed_event* _evt_sfreshness_check;
  timed_event* _evt_status_save;
  unsigned int _old_auto_rescheduling_interval;
  unsigned int _old_check_reaper_interval;
  int _old_command_check_interval;
  unsigned int _old_host_freshness_check_interval;
  unsigned int _old_retention_update_interval;
  unsigned int _old_service_freshness_check_interval;
  unsigned int _old_status_update_interval;
};
}  // namespace configuration::applier

}  // namespace com::centreon::engine

#endif  // !CCE_CONFIGURATION_APPLIER_SCHEDULER_HH
