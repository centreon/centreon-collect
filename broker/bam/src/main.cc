/**
 * Copyright 2011-2015, 2020-2024 Centreon
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

#include "bbdo/bam/ba_duration_event.hh"
#include "bbdo/bam/ba_status.hh"
#include "bbdo/bam/dimension_ba_bv_relation_event.hh"
#include "bbdo/bam/dimension_ba_event.hh"
#include "bbdo/bam/dimension_ba_timeperiod_relation.hh"
#include "bbdo/bam/dimension_bv_event.hh"
#include "bbdo/bam/dimension_kpi_event.hh"
#include "bbdo/bam/dimension_timeperiod.hh"
#include "bbdo/bam/dimension_truncate_table_signal.hh"
#include "bbdo/bam/inherited_downtime.hh"
#include "bbdo/bam/kpi_event.hh"
#include "bbdo/bam/kpi_status.hh"
#include "bbdo/bam/rebuild.hh"
#include "bbdo/events.hh"
#include "bbdo/storage/metric.hh"
#include "bbdo/storage/status.hh"
#include "com/centreon/broker/bam/factory.hh"
#include "com/centreon/broker/bam/internal.hh"
#include "com/centreon/broker/io/events.hh"
#include "com/centreon/broker/io/protocols.hh"
#include "com/centreon/broker/log_v2.hh"
#include "com/centreon/exceptions/msg_fmt.hh"

using namespace com::centreon::exceptions;
using namespace com::centreon::broker;

// Load count.
namespace {
uint32_t instances(0);
char const* bam_module("bam");
}  // namespace

template <typename T>
void register_bam_event(io::events& e, bam::data_element de, const char* name) {
  e.register_event(make_type(io::bam, de), name, &T::operations, T::entries);
}

extern "C" {
/**
 *  Module version symbol. Used to check for version mismatch.
 */
const char* broker_module_version = CENTREON_BROKER_VERSION;

/**
 * @brief Return an array with modules needed for this one to work.
 *
 * @return An array of const char*
 */
const char* const* broker_module_parents() {
  constexpr static const char* retval[]{"10-neb.so", nullptr};
  return retval;
}

/**
 *  Module deinitialization routine.
 */
bool broker_module_deinit() {
  // Decrement instance number.
  if (!--instances) {
    io::protocols::instance().unreg(bam_module);
    // Deregister bam events.
    io::events::instance().unregister_category(io::bam);
  }
  return true;  // ok to be unloaded
}

/**
 *  Module initialization routine.
 *
 *  @param[in] arg Configuration object.
 */
void broker_module_init(void const* arg) {
  (void)arg;

  // Increment instance number.
  if (!instances++) {
    // BAM module.
    log_v2::bam()->info("BAM: module for Centreon Broker {} ",
                        CENTREON_BROKER_VERSION);

    io::protocols::instance().reg(bam_module, std::make_shared<bam::factory>(),
                                  1, 7);

    io::events& e(io::events::instance());

    // Register events.
    {
      e.register_event(make_type(io::storage, storage::de_metric), "metric",
                       &storage::metric::operations, storage::metric::entries,
                       "rt_metrics");
      e.register_event(make_type(io::storage, storage::de_status), "status",
                       &storage::status::operations, storage::status::entries);

      register_bam_event<bam::ba_status>(e, bam::de_ba_status, "ba_status");
      register_bam_event<bam::kpi_status>(e, bam::de_kpi_status, "kpi_status");
      register_bam_event<bam::kpi_event>(e, bam::de_kpi_event, "kpi_event");
      register_bam_event<bam::ba_duration_event>(e, bam::de_ba_duration_event,
                                                 "ba_duration_event");
      register_bam_event<bam::dimension_ba_event>(e, bam::de_dimension_ba_event,
                                                  "dimension_ba_event");
      register_bam_event<bam::dimension_kpi_event>(
          e, bam::de_dimension_kpi_event, "dimension_kpi_event");
      register_bam_event<bam::dimension_ba_bv_relation_event>(
          e, bam::de_dimension_ba_bv_relation_event,
          "dimension_ba_bv_relation_event");
      register_bam_event<bam::dimension_bv_event>(e, bam::de_dimension_bv_event,
                                                  "dimension_bv_event");
      register_bam_event<bam::dimension_truncate_table_signal>(
          e, bam::de_dimension_truncate_table_signal,
          "dimension_truncate_table_signal");
      register_bam_event<bam::rebuild>(e, bam::de_rebuild, "rebuild");
      register_bam_event<bam::dimension_timeperiod>(
          e, bam::de_dimension_timeperiod, "dimension_timeperiod");
      register_bam_event<bam::dimension_ba_timeperiod_relation>(
          e, bam::de_dimension_ba_timeperiod_relation,
          "dimension_ba_timeperiod_relation");
      register_bam_event<bam::inherited_downtime>(e, bam::de_inherited_downtime,
                                                  "inherited_downtime");
      e.register_event(make_type(io::bam, bam::de_pb_inherited_downtime),
                       "InheritedDowntime",
                       &bam::pb_inherited_downtime::operations,
                       "InheritedDowntime");
      e.register_event(make_type(io::bam, bam::de_pb_ba_status), "BaStatus",
                       &bam::pb_ba_status::operations, "BaStatus");
      e.register_event(make_type(io::bam, bam::de_pb_ba_event), "BaEvent",
                       &bam::pb_ba_event::operations, "BaEvent");
      e.register_event(make_type(io::bam, bam::de_pb_kpi_event), "KpiEvent",
                       &bam::pb_kpi_event::operations, "KpiEvent");
      e.register_event(bam::pb_dimension_bv_event::static_type(),
                       "DimensionBvEvent",
                       &bam::pb_dimension_bv_event::operations);
      e.register_event(bam::pb_dimension_ba_bv_relation_event::static_type(),
                       "DimensionBvEvent",
                       &bam::pb_dimension_ba_bv_relation_event::operations);
      e.register_event(bam::pb_dimension_timeperiod::static_type(),
                       "DimensionTimePeriod",
                       &bam::pb_dimension_timeperiod::operations);
      e.register_event(bam::pb_dimension_ba_event::static_type(),
                       "DimensionBaEvent",
                       &bam::pb_dimension_ba_event::operations);
      e.register_event(bam::pb_dimension_kpi_event::static_type(),
                       "DimensionKpiEvent",
                       &bam::pb_dimension_kpi_event::operations);
      e.register_event(bam::pb_kpi_status::static_type(), "KpiStatus",
                       &bam::pb_kpi_status::operations);
      e.register_event(bam::pb_ba_duration_event::static_type(),
                       "BaDurationEvent",
                       &bam::pb_ba_duration_event::operations);
      e.register_event(bam::pb_dimension_ba_timeperiod_relation::static_type(),
                       "DimensionBaTimeperiodRelation",
                       &bam::pb_dimension_ba_timeperiod_relation::operations);
      e.register_event(bam::pb_dimension_truncate_table_signal::static_type(),
                       "DimensionTruncateTableSignal",
                       &bam::pb_dimension_truncate_table_signal::operations);
      e.register_event(bam::pb_services_book_state::static_type(), "BaState",
                       &bam::pb_services_book_state::operations);
      /* Let's register the ba_info event to be sure it is declared in case
       * brokerrpc is not already instanciated. */
      e.register_event(make_type(io::extcmd, extcmd::de_ba_info), "ba_info",
                       &extcmd::pb_ba_info::operations);
    }
  }
}
}
