/**
 * Copyright 2014-2017, 2021 Centreon
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

#include "com/centreon/broker/bam/configuration/reader_v2.hh"

#include <fmt/format.h>

#include "com/centreon/broker/bam/ba.hh"
#include "com/centreon/broker/bam/configuration/reader_exception.hh"
#include "com/centreon/broker/bam/configuration/state.hh"
#include "com/centreon/broker/config/applier/state.hh"
#include "com/centreon/broker/io/stream.hh"
#include "com/centreon/broker/multiplexing/publisher.hh"
#include "com/centreon/broker/sql/mysql.hh"
#include "com/centreon/broker/time/timeperiod.hh"
#include "com/centreon/exceptions/msg_fmt.hh"
#include "common/log_v2/log_v2.hh"

using namespace com::centreon::exceptions;
using namespace com::centreon::broker;
using namespace com::centreon::broker::bam::configuration;
using com::centreon::common::log_v2::log_v2;

/**
 *  Constructor.
 *
 *  @param[in] centreon_db  Centreon database connection.
 *  @param[in] storage_cfg  Storage database configuration.
 */
reader_v2::reader_v2(mysql& centreon_db, const database_config& storage_cfg)
    : _logger{log_v2::instance().get(log_v2::BAM)},
      _mysql(centreon_db),
      _storage_cfg(storage_cfg) {}

/**
 *  Read configuration from database.
 *
 *  @param[out] st  All the configuration state for the BA subsystem
 *                  recuperated from the specified database.
 */
void reader_v2::read(state& st) {
  try {
    SPDLOG_LOGGER_INFO(_logger, "loading dimensions.");
    _load_dimensions();
    SPDLOG_LOGGER_INFO(_logger, "loading BAs.");
    _load(st.get_bas(), st.get_ba_svc_mapping());
    SPDLOG_LOGGER_INFO(_logger, "loading KPIs.");
    _load(st.get_kpis());
    SPDLOG_LOGGER_INFO(_logger, "loading boolean expressions.");
    _load(st.get_bool_exps());
    SPDLOG_LOGGER_INFO(_logger, "loading mapping hosts <-> services.");
    _load(st.get_hst_svc_mapping());
    SPDLOG_LOGGER_INFO(_logger, "bam configuration loaded.");
  } catch (std::exception const& e) {
    SPDLOG_LOGGER_ERROR(_logger, "Error while reading bam configuration: {}",
                        e.what());
    st.clear();
    throw;
  }
}

/**
 *  Load KPIs from the DB.
 *
 *  @param[out] kpis The list of kpis in database.
 */
void reader_v2::_load(state::kpis& kpis) {
  try {
    std::string query(fmt::format(
        "SELECT  k.kpi_id, k.state_type, k.host_id, k.service_id, k.id_ba,"
        "        k.id_indicator_ba, k.meta_id, k.boolean_id,"
        "        k.current_status, k.downtime,"
        "        k.acknowledged, k.ignore_downtime,"
        "        k.ignore_acknowledged,"
        "        COALESCE(COALESCE(k.drop_warning, ww.impact), "
        "g.average_impact),"
        "        COALESCE(COALESCE(k.drop_critical, cc.impact), "
        "g.average_impact),"
        "        COALESCE(COALESCE(k.drop_unknown, uu.impact), "
        "g.average_impact),"
        "        k.last_state_change, k.in_downtime, k.last_impact,"
        "        kpiba.name AS ba_name, mbb.name AS boolean_name, "
        "        CONCAT(hst.host_name, '/', serv.service_description) AS "
        "service_name"
        "  FROM mod_bam_kpi AS k"
        "  INNER JOIN mod_bam AS mb"
        "    ON k.id_ba = mb.ba_id"
        "  INNER JOIN mod_bam_poller_relations AS pr"
        "    ON pr.ba_id = mb.ba_id"
        "  LEFT JOIN mod_bam_impacts AS ww"
        "    ON k.drop_warning_impact_id = ww.id_impact"
        "  LEFT JOIN mod_bam_impacts AS cc"
        "    ON k.drop_critical_impact_id = cc.id_impact"
        "  LEFT JOIN mod_bam_impacts AS uu"
        "    ON k.drop_unknown_impact_id = uu.id_impact"
        "  LEFT JOIN (SELECT id_ba, 100.0 / COUNT(kpi_id) AS average_impact"
        "               FROM mod_bam_kpi"
        "               WHERE activate='1'"
        "               GROUP BY id_ba) AS g"
        "    ON k.id_ba=g.id_ba"
        "  LEFT JOIN mod_bam as kpiba"
        "  ON k.id_indicator_ba=kpiba.ba_id"
        "  LEFT JOIN mod_bam_boolean AS mbb"
        "  ON mbb.boolean_id= k.boolean_id"
        "  LEFT JOIN service as serv"
        "  ON k.service_id = serv.service_id"
        "  LEFT JOIN host as hst"
        "  ON k.host_id = hst.host_id"
        "  WHERE k.activate='1'"
        "    AND mb.activate='1'"
        "    AND pr.poller_id={}",
        config::applier::state::instance().poller_id()));
    std::promise<database::mysql_result> promise;
    std::future<database::mysql_result> future = promise.get_future();
    _mysql.run_query_and_get_result(query, std::move(promise), 0);
    try {
      database::mysql_result res(future.get());
      while (_mysql.fetch_row(res)) {
        std::string kpi_name;
        uint32_t service_id = res.value_as_u32(3);
        uint32_t boolean_id = res.value_as_u32(7);
        uint32_t id_indicator_ba = res.value_as_u32(5);

        if (service_id > 0)
          kpi_name = "Service " + res.value_as_str(21);
        else if (boolean_id > 0)
          kpi_name = "Boolean rule " + res.value_as_str(20);
        else
          kpi_name = "Business Activity " + res.value_as_str(19);

        // KPI object.
        uint32_t kpi_id(res.value_as_u32(0));
        kpis[kpi_id] = kpi(kpi_id,                 // ID.
                           res.value_as_i32(1),    // State type.
                           res.value_as_u32(2),    // Host ID.
                           service_id,             // Service ID.
                           res.value_as_u32(4),    // BA ID.
                           id_indicator_ba,        // BA indicator ID.
                           res.value_as_u32(6),    // Meta-service ID.
                           boolean_id,             // Boolean expression ID.
                           res.value_as_i32(8),    // Status.
                           res.value_as_f32(9),    // Downtimed.
                           res.value_as_f32(10),   // Acknowledged.
                           res.value_as_bool(11),  // Ignore downtime.
                           res.value_as_bool(12),  // Ignore acknowledgement.
                           res.value_as_f64(13),   // Warning.
                           res.value_as_f64(14),   // Critical.
                           res.value_as_f64(15),   // Unknown.
                           kpi_name);

        // KPI state.
        if (!res.value_is_null(16)) {
          KpiEvent e;
          e.set_kpi_id(kpi_id);
          e.set_ba_id(res.value_as_u32(4));
          e.set_start_time(res.value_as_u64(16));
          e.set_end_time(-1);
          e.set_status(com::centreon::broker::State(res.value_as_i32(8)));
          e.set_in_downtime(res.value_as_bool(17));
          e.set_impact_level(res.value_is_null(18) ? -1 : res.value_as_f64(18));
          kpis[kpi_id].set_opened_event(e);
        }
      }
    } catch (std::exception const& e) {
      throw msg_fmt("BAM: could not retrieve KPI configuration from DB: {}",
                    e.what());
    }

    // Load host ID/service ID of meta-services (temporary fix until
    // Centreon Broker 3 where meta-services will be computed by Broker
    // itself.
    for (state::kpis::iterator it = kpis.begin(), end = kpis.end(); it != end;
         ++it) {
      if (it->second.is_meta()) {
        std::string query(fmt::format(
            "SELECT DISTINCT hsr.host_host_id, hsr.service_service_id FROM "
            "service AS s LEFT JOIN host_service_relation AS hsr ON "
            "s.service_id=hsr.service_service_id WHERE "
            "s.service_description='meta_{}'",
            it->second.get_meta_id()));
        std::promise<database::mysql_result> promise;
        std::future<database::mysql_result> future = promise.get_future();
        _mysql.run_query_and_get_result(query, std::move(promise), 0);
        try {
          database::mysql_result res(future.get());
          if (!_mysql.fetch_row(res))
            throw msg_fmt(
                "virtual service of meta-service {}"
                " does not exist",
                it->first);
          it->second.set_host_id(res.value_as_u32(0));
          it->second.set_service_id(res.value_as_u32(1));
        } catch (std::exception const& e) {
          throw msg_fmt("could not retrieve virtual meta-service's service: {}",
                        e.what());
        }
      }
    }
  } catch (reader_exception const& e) {
    (void)e;
    throw;
  } catch (std::exception const& e) {
    throw reader_exception(
        "BAM: could not retrieve KPI configuration from DB: {}", e.what());
  }
}

/**
 *  Load BAs from the DB.
 *
 *  @param[out] bas      The list of BAs in database.
 *  @param[out] mapping  The mapping of BA ID to host name/service
 *                       description.
 */
void reader_v2::_load(state::bas& bas, bam::ba_svc_mapping& mapping) {
  SPDLOG_LOGGER_INFO(_logger, "BAM: loading BAs");
  try {
    {
      std::string query(
          fmt::format("SELECT b.ba_id, b.name, b.state_source, b.level_w,"
                      " b.level_c, b.last_state_change, b.current_status,"
                      " b.in_downtime, b.inherit_kpi_downtimes"
                      " FROM mod_bam AS b"
                      " INNER JOIN mod_bam_poller_relations AS pr"
                      " ON b.ba_id=pr.ba_id"
                      " WHERE b.activate='1'"
                      " AND pr.poller_id={}",
                      config::applier::state::instance().poller_id()));
      std::promise<database::mysql_result> promise;
      std::future<database::mysql_result> future = promise.get_future();
      _mysql.run_query_and_get_result(query, std::move(promise), 0);
      try {
        database::mysql_result res(future.get());
        while (_mysql.fetch_row(res)) {
          // BA object.
          uint32_t ba_id(res.value_as_u32(0));
          bas[ba_id] = ba(ba_id,                // ID.
                          res.value_as_str(1),  // Name.
                          "", // host name
                          static_cast<configuration::ba::state_source>(
                              res.value_as_u32(2)),  // State source.
                          res.value_as_f32(3),       // Warning level.
                          res.value_as_f32(4),       // Critical level.
                          static_cast<configuration::ba::downtime_behaviour>(
                              res.value_as_u32(8)));  // Downtime inheritance.

          // BA state.
          if (!res.value_is_null(5)) {
            pb_ba_event e;
            e.mut_obj().set_ba_id(ba_id);
            e.mut_obj().set_start_time(res.value_as_u64(5));
            e.mut_obj().set_status(
                com::centreon::broker::State(res.value_as_i32(6)));
            e.mut_obj().set_in_downtime(res.value_as_bool(7));
            bas[ba_id].set_opened_event(e);
            SPDLOG_LOGGER_TRACE(
                _logger,
                "BAM: ba {} configuration (start_time:{}, in downtime: {})",
                ba_id, e.obj().start_time(), e.obj().in_downtime());
          }
        }
      } catch (std::exception const& e) {
        throw msg_fmt("BAM: {}", e.what());
      }
    }
  } catch (reader_exception const& e) {
    (void)e;
    throw;
  } catch (std::exception const& e) {
    throw reader_exception(
        "BAM: could not retrieve BA configuration from DB: {}", e.what());
  }

  // Load host_id/service_id of virtual BA services. All the associated
  // services have for description 'ba_[id]'.
  try {
    std::promise<database::mysql_result> promise;
    std::future<database::mysql_result> future = promise.get_future();
    _mysql.run_query_and_get_result(
        "SELECT DISTINCT h.host_name, s.service_description, hsr.host_host_id, "
        "hsr.service_service_id FROM service AS s INNER JOIN "
        "host_service_relation AS hsr ON s.service_id=hsr.service_service_id "
        "INNER JOIN host AS h ON hsr.host_host_id=h.host_id WHERE "
        "s.service_description LIKE 'ba\\_%'",
        std::move(promise), 0);
    database::mysql_result res(future.get());
    while (_mysql.fetch_row(res)) {
      uint32_t host_id = res.value_as_u32(2);
      uint32_t service_id = res.value_as_u32(3);
      std::string hostname = res.value_as_str(0);
      std::string service_description = res.value_as_str(1);
      service_description.erase(0, strlen("ba_"));

      if (!service_description.empty()) {
        uint32_t ba_id;
        if (!absl::SimpleAtoi(service_description, &ba_id)) {
          SPDLOG_LOGGER_INFO(
              _logger,
              "BAM: service '{}' of host '{}' is not a valid virtual BA "
              "service",
              res.value_as_str(1), res.value_as_str(0));
          continue;
        }
        state::bas::iterator found = bas.find(ba_id);
        if (found == bas.end()) {
          SPDLOG_LOGGER_INFO(
              _logger,
              "BAM: virtual BA service '{}' of host '{}' references an "
              "unknown BA ({})",
              res.value_as_str(1), res.value_as_str(0), ba_id);
          continue;
        }
        found->second.set_host_id(host_id);
        found->second.set_service_id(service_id);
        found->second.set_host_name(hostname);
        mapping.set(ba_id, res.value_as_str(0), res.value_as_str(1));
      }
    }
  } catch (reader_exception const& e) {
    (void)e;
    throw;
  } catch (std::exception const& e) {
    throw reader_exception("BAM: could not retrieve BA service IDs from DB: {}",
                           e.what());
  }

  // Test for BA without service ID.
  for (state::bas::const_iterator it = bas.begin(), end = bas.end(); it != end;
       ++it)
    if (it->second.get_service_id() == 0)
      throw reader_exception("BAM: BA {} has no associated service",
                             it->second.get_id());
}

/**
 *  Load boolean expressions from the DB.
 *
 *  @param[out] bool_exps The list of bool expression in database.
 */
void reader_v2::_load(state::bool_exps& bool_exps) {
  // Load boolean expressions themselves.
  try {
    std::string query(
        fmt::format("SELECT b.boolean_id, b.name, b.expression, b.bool_state"
                    " FROM mod_bam_boolean AS b"
                    " INNER JOIN mod_bam_kpi AS k"
                    " ON b.boolean_id=k.boolean_id"
                    " INNER JOIN mod_bam_poller_relations AS pr"
                    " ON k.id_ba=pr.ba_id"
                    " WHERE b.activate=1"
                    " AND pr.poller_id={}",
                    config::applier::state::instance().poller_id()));
    std::promise<database::mysql_result> promise;
    std::future<database::mysql_result> future = promise.get_future();
    _mysql.run_query_and_get_result(query, std::move(promise), 0);
    database::mysql_result res(future.get());
    while (_mysql.fetch_row(res)) {
      bool_exps[res.value_as_u32(0)] =
          bool_expression(res.value_as_u32(0),    // ID.
                          res.value_as_str(1),    // Name.
                          res.value_as_str(2),    // Expression.
                          res.value_as_bool(3));  // Impact if.
    }
  } catch (reader_exception const& e) {
    (void)e;
    throw;
  } catch (std::exception const& e) {
    throw reader_exception(
        "BAM: could not retrieve boolean expression "
        "configuration from DB: {}",
        e.what());
  }
}

/**
 *  Load host/service IDs from the DB.
 *
 *  @param[out] mapping  Host/service mapping.
 */
void reader_v2::_load(bam::hst_svc_mapping& mapping) {
  try {
    // XXX : expand hostgroups and servicegroups
    std::promise<database::mysql_result> promise;
    std::future<database::mysql_result> future = promise.get_future();
    _mysql.run_query_and_get_result(
        "SELECT DISTINCT h.host_id, s.service_id, h.host_name, "
        "s.service_description,service_activate FROM service s LEFT JOIN "
        "host_service_relation hsr ON s.service_id=hsr.service_service_id LEFT "
        "JOIN host h ON hsr.host_host_id=h.host_id",
        std::move(promise), 0);
    database::mysql_result res(future.get());
    while (_mysql.fetch_row(res))
      mapping.set_service(res.value_as_str(2), res.value_as_str(3),
                          res.value_as_u32(0), res.value_as_u32(1),
                          res.value_as_str(4) == "1");
  } catch (reader_exception const& e) {
    (void)e;
    throw;
  } catch (std::exception const& e) {
    throw reader_exception("BAM: could not retrieve host/service IDs: {}",
                           e.what());
  }
}

/**
 *  Load the dimensions from the database.
 */
void reader_v2::_load_dimensions() {
  SPDLOG_LOGGER_TRACE(_logger, "load dimensions");
  auto out{std::make_unique<multiplexing::publisher>()};
  // As this operation is destructive (it truncates the database),
  // we cache the data until we are sure we have all the data
  // needed from the database.
  std::deque<std::shared_ptr<io::data>> datas;
  auto trunc_event = std::make_shared<pb_dimension_truncate_table_signal>();
  trunc_event->mut_obj().set_update_started(true);
  datas.emplace_back(trunc_event);

  // Load the dimensions.
  std::unordered_map<uint32_t, std::shared_ptr<pb_dimension_ba_event>> bas;

  // Load the timeperiods themselves.
  std::promise<database::mysql_result> promise_tp;
  std::future<database::mysql_result> future_tp = promise_tp.get_future();

  _mysql.run_query_and_get_result(
      "SELECT tp_id, tp_name, tp_sunday, tp_monday, tp_tuesday, "
      "tp_wednesday, tp_thursday, tp_friday, tp_saturday"
      " FROM timeperiod",
      std::move(promise_tp), 0);

  // Load the BAs.
  std::string query_ba(
      fmt::format("SELECT b.ba_id, b.name, b.description,"
                  " b.sla_month_percent_warn, b.sla_month_percent_crit,"
                  " b.sla_month_duration_warn,"
                  " b.sla_month_duration_crit, b.id_reporting_period"
                  " FROM mod_bam AS b"
                  " INNER JOIN mod_bam_poller_relations AS pr"
                  " ON b.ba_id=pr.ba_id"
                  " WHERE b.activate='1'"
                  " AND pr.poller_id={}",
                  config::applier::state::instance().poller_id()));
  std::promise<database::mysql_result> promise_ba;
  std::future<database::mysql_result> future_ba = promise_ba.get_future();
  _mysql.run_query_and_get_result(query_ba, std::move(promise_ba), 0);

  // Load the BVs.
  std::promise<database::mysql_result> promise_bv;
  std::future<database::mysql_result> future_bv = promise_bv.get_future();
  _mysql.run_query_and_get_result(
      "SELECT id_ba_group, ba_group_name, ba_group_description"
      " FROM mod_bam_ba_groups",
      std::move(promise_bv), 0);

  // Load the BA BV relations.
  std::string query(
      fmt::format("SELECT id_ba, id_ba_group"
                  " FROM mod_bam_bagroup_ba_relation as r"
                  " INNER JOIN mod_bam AS b"
                  " ON b.ba_id = r.id_ba"
                  " INNER JOIN mod_bam_poller_relations AS pr"
                  " ON b.ba_id=pr.ba_id"
                  " WHERE b.activate='1'"
                  " AND pr.poller_id={}",
                  config::applier::state::instance().poller_id()));
  std::promise<database::mysql_result> promise_ba_bv;
  std::future<database::mysql_result> future_ba_bv = promise_ba_bv.get_future();
  _mysql.run_query_and_get_result(query, std::move(promise_ba_bv), 0);

  // Load the KPIs
  // Unfortunately, we need to get the names of the
  // service/host/meta_service/ba/boolean expression associated with
  // this KPI. This explains the numerous joins.
  std::string query_kpi{
      fmt::format("SELECT k.kpi_id, k.kpi_type, k.host_id, k.service_id,"
                  "       k.id_ba, k.id_indicator_ba, k.meta_id,"
                  "       k.boolean_id,"
                  "       COALESCE(COALESCE(k.drop_warning, ww.impact), "
                  "g.average_impact),"
                  "       COALESCE(COALESCE(k.drop_critical, cc.impact), "
                  "g.average_impact),"
                  "       COALESCE(COALESCE(k.drop_unknown, uu.impact), "
                  "g.average_impact),"
                  "       h.host_name, s.service_description, b.name,"
                  "       meta.meta_name, boo.name"
                  "  FROM mod_bam_kpi AS k"
                  "  LEFT JOIN mod_bam_impacts AS ww"
                  "    ON k.drop_warning_impact_id = ww.id_impact"
                  "  LEFT JOIN mod_bam_impacts AS cc"
                  "    ON k.drop_critical_impact_id = cc.id_impact"
                  "  LEFT JOIN mod_bam_impacts AS uu"
                  "    ON k.drop_unknown_impact_id = uu.id_impact"
                  "  LEFT JOIN host AS h"
                  "    ON h.host_id = k.host_id"
                  "  LEFT JOIN service AS s"
                  "    ON s.service_id = k.service_id"
                  "  INNER JOIN mod_bam AS b"
                  "    ON b.ba_id = k.id_ba"
                  "  INNER JOIN mod_bam_poller_relations AS pr"
                  "    ON b.ba_id = pr.ba_id"
                  "  LEFT JOIN meta_service AS meta"
                  "    ON meta.meta_id = k.meta_id"
                  "  LEFT JOIN mod_bam_boolean as boo"
                  "    ON boo.boolean_id = k.boolean_id"
                  "  LEFT JOIN (SELECT id_ba, 100.0 / COUNT(kpi_id) AS "
                  "average_impact"
                  "               FROM mod_bam_kpi"
                  "               WHERE activate='1'"
                  "               GROUP BY id_ba) AS g"
                  "   ON k.id_ba=g.id_ba"
                  "  WHERE k.activate='1'"
                  "    AND b.activate='1'"
                  "    AND pr.poller_id={}",
                  config::applier::state::instance().poller_id())};
  std::promise<database::mysql_result> promise_kpi;
  std::future<database::mysql_result> future_kpi = promise_kpi.get_future();
  _mysql.run_query_and_get_result(query_kpi, std::move(promise_kpi), 0);

  // Load the ba-timeperiods relations.
  std::promise<database::mysql_result> promise_ba_tp;
  std::future<database::mysql_result> future_ba_tp = promise_ba_tp.get_future();
  _mysql.run_query_and_get_result(
      fmt::format(
          "SELECT bt.ba_id, bt.tp_id FROM mod_bam_relations_ba_timeperiods bt"
          " INNER JOIN mod_bam AS b"
          " ON b.ba_id = bt.ba_id"
          " INNER JOIN mod_bam_poller_relations AS pr"
          " ON b.ba_id=pr.ba_id"
          " WHERE b.activate='1'"
          " AND pr.poller_id={}",
          config::applier::state::instance().poller_id()),
      std::move(promise_ba_tp), 0);

  try {
    database::mysql_result res(future_tp.get());
    while (_mysql.fetch_row(res)) {
      auto tp(std::make_shared<pb_dimension_timeperiod>());
      auto& data = tp->mut_obj();
      data.set_id(res.value_as_u32(0));
      data.set_name(res.value_as_str(1));
      data.set_sunday(res.value_as_str(2));
      data.set_monday(res.value_as_str(3));
      data.set_tuesday(res.value_as_str(4));
      data.set_wednesday(res.value_as_str(5));
      data.set_thursday(res.value_as_str(6));
      data.set_friday(res.value_as_str(7));
      data.set_saturday(res.value_as_str(8));
      datas.push_back(tp);
    }
  } catch (std::exception const& e) {
    throw reader_exception(
        "BAM: could not load some dimension table: "
        "could not load timeperiods from the database: {}",
        e.what());
  }

  try {
    database::mysql_result res(future_ba.get());
    while (_mysql.fetch_row(res)) {
      auto ba{std::make_shared<pb_dimension_ba_event>()};
      DimensionBaEvent& ba_pb = ba->mut_obj();
      ba_pb.set_ba_id(res.value_as_u32(0));
      ba_pb.set_ba_name(res.value_as_str(1));
      ba_pb.set_ba_description(res.value_as_str(2));
      ba_pb.set_sla_month_percent_warn(res.value_as_f64(3));
      ba_pb.set_sla_month_percent_crit(res.value_as_f64(4));
      ba_pb.set_sla_duration_warn(res.value_as_i32(5));
      ba_pb.set_sla_duration_crit(res.value_as_i32(6));
      datas.push_back(ba);
      bas[ba_pb.ba_id()] = ba;
      if (!res.value_is_null(7)) {
        std::shared_ptr<pb_dimension_ba_timeperiod_relation> dbtr(
            std::make_shared<pb_dimension_ba_timeperiod_relation>());
        dbtr->mut_obj().set_ba_id(res.value_as_u32(0));
        dbtr->mut_obj().set_timeperiod_id(res.value_as_u32(7));
        dbtr->mut_obj().set_is_default(true);
        datas.push_back(dbtr);
      }
    }
  } catch (std::exception const& e) {
    throw reader_exception(
        "BAM: could not load some dimension table: "
        "could not retrieve BAs from the database: {}",
        e.what());
  }

  try {
    database::mysql_result res(future_bv.get());
    while (_mysql.fetch_row(res)) {
      std::shared_ptr<pb_dimension_bv_event> bv(
          std::make_shared<pb_dimension_bv_event>());
      bv->mut_obj().set_bv_id(res.value_as_u32(0));
      bv->mut_obj().set_bv_name(res.value_as_str(1));
      bv->mut_obj().set_bv_description(res.value_as_str(2));
      datas.push_back(bv);
    }
  } catch (std::exception const& e) {
    throw reader_exception(
        "BAM: could not load some dimension table: "
        "could not retrieve BVs from the database: {}",
        e.what());
  }

  try {
    database::mysql_result res(future_ba_bv.get());
    while (_mysql.fetch_row(res)) {
      std::shared_ptr<pb_dimension_ba_bv_relation_event> babv(
          std::make_shared<pb_dimension_ba_bv_relation_event>());
      babv->mut_obj().set_ba_id(res.value_as_u32(0));
      babv->mut_obj().set_bv_id(res.value_as_u32(1));
      datas.push_back(babv);
    }
  } catch (const std::exception& e) {
    throw reader_exception(
        "BAM: could not load some dimension table: "
        "could not retrieve BV memberships of BAs: {}",
        e.what());
  }

  try {
    database::mysql_result res(future_kpi.get());

    while (_mysql.fetch_row(res)) {
      auto k{std::make_shared<pb_dimension_kpi_event>()};
      DimensionKpiEvent& ev = k->mut_obj();
      ev.set_kpi_id(res.value_as_u32(0));
      ev.set_host_id(res.value_as_u32(2));
      ev.set_service_id(res.value_as_u32(3));
      ev.set_ba_id(res.value_as_u32(4));
      ev.set_kpi_ba_id(res.value_as_u32(5));
      ev.set_meta_service_id(res.value_as_u32(6));
      ev.set_boolean_id(res.value_as_u32(7));
      ev.set_impact_warning(res.value_as_f64(8));
      ev.set_impact_critical(res.value_as_f64(9));
      ev.set_impact_unknown(res.value_as_f64(10));
      ev.set_host_name(res.value_as_str(11));
      ev.set_service_description(res.value_as_str(12));
      ev.set_ba_name(res.value_as_str(13));
      ev.set_meta_service_name(res.value_as_str(14));
      ev.set_boolean_name(res.value_as_str(15));

      // Resolve the id_indicator_ba.
      if (ev.kpi_ba_id()) {
        auto found = bas.find(ev.kpi_ba_id());
        if (found == bas.end()) {
          SPDLOG_LOGGER_ERROR(_logger,
                              "BAM: could not retrieve BA {} used as KPI {} in "
                              "dimension table: ignoring this KPI",
                              ev.kpi_ba_id(), ev.kpi_id());
          continue;
        }
        ev.set_kpi_ba_name(found->second->obj().ba_name());
      }
      datas.push_back(k);
    }
  } catch (std::exception const& e) {
    throw reader_exception(
        "BAM: could not load some dimension table: "
        "could not retrieve KPI dimensions: {}",
        e.what());
  }

  try {
    database::mysql_result res(future_ba_tp.get());
    while (_mysql.fetch_row(res)) {
      std::shared_ptr<pb_dimension_ba_timeperiod_relation> dbtr(
          std::make_shared<pb_dimension_ba_timeperiod_relation>());
      dbtr->mut_obj().set_ba_id(res.value_as_u32(0));
      dbtr->mut_obj().set_timeperiod_id(res.value_as_u32(1));
      dbtr->mut_obj().set_is_default(false);
      datas.push_back(dbtr);
    }
  } catch (std::exception const& e) {
    throw reader_exception(
        "BAM: could not load some dimension table: could not retrieve the "
        "timeperiods associated with the BAs: {}",
        e.what());
  }

  // End the update.
  datas.emplace_back(std::make_shared<pb_dimension_truncate_table_signal>());

  // Write all the cached data to the publisher.
  for (auto& e : datas)
    out->write(e);
}
