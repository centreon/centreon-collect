/**
 * Copyright 2014-2015, 2021, 2023 Centreon
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

#include "com/centreon/broker/bam/connector.hh"

#include "bbdo/bam/ba_status.hh"
#include "bbdo/bam/dimension_ba_bv_relation_event.hh"
#include "bbdo/bam/dimension_ba_event.hh"
#include "bbdo/bam/dimension_ba_timeperiod_relation.hh"
#include "bbdo/bam/dimension_bv_event.hh"
#include "bbdo/bam/dimension_kpi_event.hh"
#include "bbdo/bam/dimension_timeperiod.hh"
#include "bbdo/bam/dimension_truncate_table_signal.hh"
#include "bbdo/bam/kpi_event.hh"
#include "bbdo/bam/kpi_status.hh"
#include "bbdo/bam/rebuild.hh"

#include "com/centreon/broker/bam/monitoring_stream.hh"
#include "com/centreon/broker/bam/reporting_stream.hh"
#include "com/centreon/broker/neb/acknowledgement.hh"
#include "com/centreon/broker/neb/downtime.hh"
#include "com/centreon/broker/neb/service.hh"

using namespace com::centreon::broker;
using namespace com::centreon::broker::bam;

static constexpr multiplexing::muxer_filter _monitoring_stream_filter = {
    neb::service_status::static_type(),
    neb::pb_service_status::static_type(),
    neb::service::static_type(),
    neb::pb_service::static_type(),
    neb::acknowledgement::static_type(),
    neb::pb_acknowledgement::static_type(),
    neb::downtime::static_type(),
    neb::pb_downtime::static_type(),
    neb::pb_adaptive_service_status::static_type(),
    bam::ba_status::static_type(),
    bam::pb_ba_status::static_type(),
    bam::kpi_status::static_type(),
    bam::pb_kpi_status::static_type(),
    inherited_downtime::static_type(),
    pb_inherited_downtime::static_type(),
    extcmd::pb_ba_info::static_type(),
    pb_services_book_state::static_type()};

static constexpr multiplexing::muxer_filter _monitoring_forbidden_filter =
    multiplexing::muxer_filter(_monitoring_stream_filter).reverse();

static constexpr multiplexing::muxer_filter _reporting_stream_filter = {
    bam::kpi_event::static_type(),
    bam::pb_kpi_event::static_type(),
    bam::ba_event::static_type(),
    bam::pb_ba_event::static_type(),
    bam::ba_duration_event::static_type(),
    bam::pb_ba_duration_event::static_type(),
    bam::dimension_truncate_table_signal::static_type(),
    bam::pb_dimension_truncate_table_signal::static_type(),
    bam::dimension_ba_event::static_type(),
    bam::pb_dimension_ba_event::static_type(),
    bam::dimension_bv_event::static_type(),
    bam::pb_dimension_bv_event::static_type(),
    bam::dimension_ba_bv_relation_event::static_type(),
    bam::pb_dimension_ba_bv_relation_event::static_type(),
    bam::dimension_kpi_event::static_type(),
    bam::pb_dimension_kpi_event::static_type(),
    bam::dimension_timeperiod::static_type(),
    bam::pb_dimension_timeperiod::static_type(),
    bam::dimension_ba_timeperiod_relation::static_type(),
    bam::pb_dimension_ba_timeperiod_relation::static_type(),
    bam::rebuild::static_type()};

static constexpr multiplexing::muxer_filter _reporting_forbidden_filter =
    multiplexing::muxer_filter(_reporting_stream_filter).reverse();

/**
 * @brief Constructor. This function is not easy to use so it is private and
 * called thanks two static functions:
 * * create_monitoring_connector()
 * * create_reporting_connector()
 *
 * @param type A stream_type enum giving the following choices :
 * bam_monitoring_type or bam_reporting_type.
 * @param db_cfg The database configuration.
 * @param mandatory_filter The mandatory filters of the underlying stream.
 * @param forbidden_filter The forbidden filters of the underlying stream.
 */
connector::connector(stream_type type,
                     const database_config& db_cfg,
                     const multiplexing::muxer_filter& mandatory_filter,
                     const multiplexing::muxer_filter& forbidden_filter)
    : io::endpoint(false, mandatory_filter, forbidden_filter),
      _type{type},
      _db_cfg{db_cfg} {}

/**
 * @brief Static function to create a connector for a bam monitoring stream.
 *
 * @param ext_cmd_file The external command file to connect to Centreon Engine.
 * @param db_cfg The database configuration.
 * @param storage_db_name The storage database name.
 * @param cache The persistent cache.
 *
 * @return An unique ptr to the newly bam connector created.
 */
std::unique_ptr<bam::connector> connector::create_monitoring_connector(
    const std::string& ext_cmd_file,
    const database_config& db_cfg,
    const std::string& storage_db_name,
    std::shared_ptr<persistent_cache> cache) {
  auto retval = std::unique_ptr<bam::connector>(
      new bam::connector(bam_monitoring_type, db_cfg, _monitoring_stream_filter,
                         _monitoring_forbidden_filter));
  retval->_ext_cmd_file = ext_cmd_file;
  retval->_cache = std::move(cache);
  if (storage_db_name.empty())
    retval->_storage_db_name = db_cfg.get_name();
  else
    retval->_storage_db_name = storage_db_name;
  return retval;
}

/**
 *  Static function to create a connector for a bam reporting stream.
 *
 *  @param[in] db_cfg  Database configuration.
 */
std::unique_ptr<bam::connector> connector::create_reporting_connector(
    const database_config& db_cfg) {
  auto retval = std::unique_ptr<bam::connector>(
      new bam::connector(bam_reporting_type, db_cfg, _reporting_stream_filter,
                         _reporting_forbidden_filter));
  return retval;
}

/**
 * @brief Connect to a DB.
 *
 * @return BAM connection object.
 */
std::shared_ptr<io::stream> connector::open() {
  auto logger = log_v2::instance().get(log_v2::BAM);
  if (_type == bam_reporting_type)
    return std::make_shared<bam::reporting_stream>(_db_cfg, logger);
  else {
    database_config storage_db_cfg(_db_cfg);
    storage_db_cfg.set_name(_storage_db_name);
    auto u = std::make_shared<monitoring_stream>(
        _ext_cmd_file, _db_cfg, storage_db_cfg, _cache, logger);
    // FIXME DBR: just after this creation, initialize() is called by update()
    // So I think this call is not needed. But for now not totally sure.
    // u->initialize();
    return u;
  }
}
