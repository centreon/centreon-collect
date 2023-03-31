/*
** Copyright 2014-2015, 2021 Centreon
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
**
** For more information : contact@centreon.com
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
#include "com/centreon/broker/neb/service_status.hh"

using namespace com::centreon::broker;
using namespace com::centreon::broker::bam;

static constexpr multiplexing::muxer_filter _monitoring_stream_filter = {
    neb::service_status::static_type(),  neb::pb_service_status::static_type(),
    neb::service::static_type(),         neb::pb_service::static_type(),
    neb::acknowledgement::static_type(), neb::pb_acknowledgement::static_type(),
    neb::downtime::static_type(),        neb::pb_downtime::static_type(),
    bam::ba_status::static_type(),       bam::pb_ba_status::static_type(),
    bam::kpi_status::static_type(),      bam::pb_kpi_status::static_type(),
    inherited_downtime::static_type(),   pb_inherited_downtime::static_type()};

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
/**
 *  Default constructor.
 */
connector::connector() : io::endpoint(false), _type(bam_monitoring_type) {}

/**
 *  Set connection parameters.
 *
 *  @param[in] ext_cmd_file     The external command file to connect to Centreon
 * Engine.
 *  @param[in] db_cfg           Database configuration.
 *  @param[in] storage_db_name  Storage database name.
 *  @param[in] cache            The persistent cache.
 */
void connector::connect_monitoring(std::string const& ext_cmd_file,
                                   database_config const& db_cfg,
                                   std::string const& storage_db_name,
                                   std::shared_ptr<persistent_cache> cache) {
  _type = bam_monitoring_type;
  _ext_cmd_file = ext_cmd_file;
  _db_cfg = db_cfg;
  _cache = std::move(cache);
  if (storage_db_name.empty())
    _storage_db_name = db_cfg.get_name();
  else
    _storage_db_name = storage_db_name;
  _muxer_filter = _monitoring_stream_filter;
}

/**
 *  Set reporting connection parameters.
 *
 *  @param[in] db_cfg  Database configuration.
 */
void connector::connect_reporting(database_config const& db_cfg) {
  _type = bam_reporting_type;
  _db_cfg = db_cfg;
  _storage_db_name.clear();
  _muxer_filter = _reporting_stream_filter;
}

/**
 * @brief Connect to a DB.
 *
 * @return BAM connection object.
 */
std::unique_ptr<io::stream> connector::open() {
  if (_type == bam_reporting_type)
    return std::unique_ptr<io::stream>(new reporting_stream(_db_cfg));
  else {
    database_config storage_db_cfg(_db_cfg);
    storage_db_cfg.set_name(_storage_db_name);
    auto u = std::make_unique<monitoring_stream>(_ext_cmd_file, _db_cfg,
                                                 storage_db_cfg, _cache);
    // FIXME DBR: just after this creation, initialize() is called by update()
    // So I think this call is not needed. But for now not totally sure.
    // u->initialize();
    return u;
  }
}
