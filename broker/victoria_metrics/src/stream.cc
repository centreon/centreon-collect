/**
 * Copyright 2022-2024 Centreon
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

#include "com/centreon/broker/victoria_metrics/stream.hh"
#include "com/centreon/broker/cache/global_cache.hh"
#include "com/centreon/broker/victoria_metrics/request.hh"
#include "bbdo/storage/metric.hh"
#include "bbdo/storage/remove_graph.hh"
#include "bbdo/storage/status.hh"
#include "com/centreon/exceptions/msg_fmt.hh"
#include "common/crypto/base64.hh"
#include "common/log_v2/log_v2.hh"

#include <absl/container/flat_hash_set.h>
#include <absl/strings/ascii.h>
#include <absl/strings/match.h>
#include <absl/strings/numbers.h>
#include <absl/strings/str_cat.h>
#include <absl/strings/str_join.h>
#include <absl/strings/str_split.h>
#include <fmt/ranges.h>
#include <nlohmann/json.hpp>

using namespace com::centreon::broker;
using namespace com::centreon::broker::victoria_metrics;
using namespace com::centreon::common::crypto;
using com::centreon::exceptions::msg_fmt;
using log_v2 = com::centreon::common::log_v2::log_v2;

// selectors per delete or series request, to keep bodies small
static constexpr size_t _selectors_per_request = 100;

/************************************************************************
 *      metric_labels
 ************************************************************************/

void metric_labels::set(uint64_t metric_id,
                        const std::string& name,
                        const std::string& unit) {
  absl::MutexLock l(_protect);
  labels& to_update = _labels[metric_id];
  if (to_update.name != name)
    to_update.name = name;
  if (to_update.unit != unit)
    to_update.unit = unit;
}

std::optional<metric_labels::labels> metric_labels::get(
    uint64_t metric_id) const {
  absl::MutexLock l(_protect);
  auto search = _labels.find(metric_id);
  if (search == _labels.end())
    return std::nullopt;
  return search->second;
}

void metric_labels::erase(uint64_t metric_id) {
  absl::MutexLock l(_protect);
  _labels.erase(metric_id);
}

/************************************************************************
 *      stream
 ************************************************************************/

const std::string stream::allowed_macros =
    "$INSTANCE$,$INSTANCEID$,$HOST$,$SERVICE$,$HOSTGROUP$,$SERVICE_GROUP$,"
    "$RESOURCEID$,"
    "$HOST_TAG_CAT_ID$,$HOST_TAG_GROUP_ID$,$HOST_TAG_CAT_NAME$,"
    "$HOST_TAG_GROUP_NAME$,$SERV_TAG_CAT_ID$,$SERV_TAG_GROUP_ID$,"
    "$SERV_TAG_CAT_NAME$,$SERV_TAG_GROUP_NAME$,$MIN$,$MAX$";

stream::stream(const std::shared_ptr<asio::io_context>& io_context,
               const std::shared_ptr<http_tsdb::http_tsdb_config>& conf,
               const std::string& account_id,
               http::connection_creator conn_creator,
               const series_config& series_conf)
    : http_tsdb::stream("victoria_metrics",
                        io_context,
                        log_v2::instance().get(log_v2::VICTORIA_METRICS),
                        conf,
                        conn_creator),
      _metric_formatter(allowed_macros,
                        conf->get_metric_columns(),
                        http_tsdb::line_protocol_query::data_type::metric,
                        _logger),
      _status_formatter(allowed_macros,
                        conf->get_status_columns(),
                        http_tsdb::line_protocol_query::data_type::status,
                        _logger),
      _account_id(account_id),
      _write_target(write_target_with_precision(conf->get_http_target())),
      _series_conf(series_conf) {
  // in order to avoid reallocation of request body
  _body_size_to_reserve = conf->get_max_queries_per_transaction() *
                          (128 + std::max(conf->get_metric_columns().size(),
                                          conf->get_status_columns().size()) *
                                     20);

  _authorization = "Basic ";
  _authorization += base64_encode(conf->get_user() + ':' + conf->get_pwd());
}

std::shared_ptr<stream> stream::load(
    const std::shared_ptr<asio::io_context>& io_context,
    const std::shared_ptr<http_tsdb::http_tsdb_config>& conf,
    const std::string& account_id,
    http::connection_creator conn_creator,
    const series_config& series_conf) {
  return std::shared_ptr<stream>(
      new stream(io_context, conf, account_id, conn_creator, series_conf));
}

std::string stream::write_target_with_precision(const std::string& target) {
  size_t query = target.find('?');
  if (query == std::string::npos)
    return target + "?precision=s";
  std::string_view params(target);
  params.remove_prefix(query + 1);
  for (std::string_view param : absl::StrSplit(params, '&')) {
    if (absl::StartsWith(param, "precision="))
      return target;
  }
  return target + (params.empty() ? "precision=s" : "&precision=s");
}

std::string stream::form_encode(const std::string& value) {
  std::string ret;
  ret.reserve(value.size() * 3);
  for (unsigned char c : value) {
    if (absl::ascii_isalnum(c) || c == '-' || c == '_' || c == '.' ||
        c == '~')
      ret.push_back(c);
    else
      fmt::format_to(std::back_inserter(ret), "%{:02X}", c);
  }
  return ret;
}

http_tsdb::request::pointer stream::create_request() const {
  auto ret = std::make_shared<request>(
      boost::beast::http::verb::post, _conf->get_server_name(), _write_target,
      _logger, _body_size_to_reserve, _metric_formatter, _status_formatter,
      _authorization);

  ret->set(boost::beast::http::field::content_type, "text/plain");
  ret->set(boost::beast::http::field::accept, "application/json");
  ret->set(boost::beast::http::field::accept_encoding, "gzip");
  ret->set("account_id", _account_id);

  return ret;
}

std::shared_ptr<http::request_base> stream::_create_form_request(
    const std::string& target,
    const std::vector<std::string>& selectors,
    size_t from,
    size_t to) const {
  auto ret = std::make_shared<http::request_base>(
      boost::beast::http::verb::post, _conf->get_server_name(), target);
  ret->set(boost::beast::http::field::authorization, _authorization);
  ret->set(boost::beast::http::field::content_type,
           "application/x-www-form-urlencoded");
  ret->set(boost::beast::http::field::accept, "application/json");
  ret->set("account_id", _account_id);
  std::string& body = ret->body();
  for (size_t i = from; i < to; ++i) {
    if (!body.empty())
      body.push_back('&');
    absl::StrAppend(&body, "match%5B%5D=", form_encode(selectors[i]));
  }
  return ret;
}

int stream::_ack_one() {
  std::lock_guard<std::mutex> l(_protect);
  ++_acknowledged;
  int ret = _acknowledged;
  _acknowledged = 0;
  return ret;
}

int stream::write(const std::shared_ptr<io::data>& d) {
  if (!validate(d, get_name()))
    return http_tsdb::stream::write(d);

  switch (d->type()) {
    case storage::pb_metric::static_type(): {
      const Metric& m = std::static_pointer_cast<storage::pb_metric>(d)->obj();
      _series_conf.labels->set(m.metric_id(), m.name(), m.unit());
    } break;
    case storage::metric::static_type(): {
      const storage::metric& m = *std::static_pointer_cast<storage::metric>(d);
      _series_conf.labels->set(m.metric_id, m.name, "");
    } break;
    case storage::pb_rebuild_message::static_type(): {
      const RebuildMessage& msg =
          std::static_pointer_cast<storage::pb_rebuild_message>(d)->obj();
      switch (msg.state()) {
        case RebuildMessage_State_START:
          _rebuild_start(msg);
          break;
        case RebuildMessage_State_DATA:
          _rebuild_data(msg);
          break;
        case RebuildMessage_State_END:
          _rebuild_end(msg);
          break;
        default:
          SPDLOG_LOGGER_ERROR(_logger,
                              "victoria_metrics: bad state {} in rebuild "
                              "message",
                              static_cast<int>(msg.state()));
      }
      return _ack_one();
    }
    case storage::pb_remove_graph_message::static_type(): {
      const RemoveGraphMessage& msg =
          std::static_pointer_cast<storage::pb_remove_graph_message>(d)->obj();
      std::vector<std::string> selectors;
      for (uint64_t metric_id : msg.metric_ids())
        selectors.emplace_back(
            fmt::format("metric_val{{id=\"{}\"}}", metric_id));
      for (uint64_t index_id : msg.index_ids()) {
        selectors.emplace_back(
            fmt::format("metric_val{{index_id=\"{}\"}}", index_id));
        selectors.emplace_back(
            fmt::format("status_val{{id=\"{}\"}}", index_id));
      }
      SPDLOG_LOGGER_INFO(_logger,
                         "victoria_metrics: removing series of metrics ({}) "
                         "and indexes ({})",
                         fmt::join(msg.metric_ids(), ","),
                         fmt::join(msg.index_ids(), ","));
      drain_requests();
      _delete_series(selectors);
      for (uint64_t metric_id : msg.metric_ids())
        _series_conf.labels->erase(metric_id);
      return _ack_one();
    }
    case storage::remove_graph::static_type(): {
      const storage::remove_graph& e =
          *std::static_pointer_cast<storage::remove_graph>(d);
      std::vector<std::string> selectors;
      if (e.is_index) {
        selectors.emplace_back(
            fmt::format("metric_val{{index_id=\"{}\"}}", e.id));
        selectors.emplace_back(fmt::format("status_val{{id=\"{}\"}}", e.id));
      } else {
        selectors.emplace_back(fmt::format("metric_val{{id=\"{}\"}}", e.id));
        _series_conf.labels->erase(e.id);
      }
      SPDLOG_LOGGER_INFO(_logger, "victoria_metrics: removing series of {} {}",
                         e.is_index ? "index" : "metric", e.id);
      drain_requests();
      _delete_series(selectors);
      return _ack_one();
    }
    default:
      break;
  }
  return http_tsdb::stream::write(d);
}

void stream::_delete_series(const std::vector<std::string>& selectors) {
  for (size_t from = 0; from < selectors.size();
       from += _selectors_per_request) {
    size_t to = std::min(selectors.size(), from + _selectors_per_request);
    send_sync(_create_form_request(_series_conf.delete_target, selectors, from,
                                   to));
    SPDLOG_LOGGER_DEBUG(_logger, "victoria_metrics: series {} deleted",
                        fmt::join(selectors.begin() + from,
                                  selectors.begin() + to, " "));
  }
}

void stream::_resolve_labels(const std::vector<uint64_t>& metric_ids) {
  std::vector<std::string> selectors;
  selectors.reserve(metric_ids.size());
  for (uint64_t metric_id : metric_ids)
    selectors.emplace_back(fmt::format("metric_val{{id=\"{}\"}}", metric_id));

  for (size_t from = 0; from < selectors.size();
       from += _selectors_per_request) {
    size_t to = std::min(selectors.size(), from + _selectors_per_request);
    auto req =
        _create_form_request(_series_conf.series_target, selectors, from, to);
    // the series API only looks at the last day by default
    req->body() += "&start=1";
    http::response_ptr resp = send_sync(req);
    try {
      nlohmann::json answer = nlohmann::json::parse(resp->body());
      for (const nlohmann::json& series : answer.at("data")) {
        auto id = series.find("id");
        auto name = series.find("name");
        uint64_t metric_id;
        if (id == series.end() || name == series.end() ||
            !absl::SimpleAtoi(id->get<std::string>(), &metric_id))
          continue;
        auto unit = series.find("unit");
        _series_conf.labels->set(
            metric_id, name->get<std::string>(),
            unit == series.end() ? std::string() : unit->get<std::string>());
      }
    } catch (const std::exception& e) {
      throw msg_fmt("victoria_metrics: bad answer of {}: {}",
                    _series_conf.series_target, e.what());
    }
  }
}

void stream::_rebuild_start(const RebuildMessage& msg) {
  if (msg.metric_to_index_id().empty()) {
    SPDLOG_LOGGER_ERROR(_logger, "victoria_metrics: rebuild of no metric");
    return;
  }
  SPDLOG_LOGGER_INFO(_logger,
                     "victoria_metrics: starting to rebuild metrics ({})",
                     absl::StrJoin(msg.metric_to_index_id(), ",",
                                   absl::PairFormatter(":")));
  // live points already sent must not land after the deletion
  drain_requests();

  std::vector<uint64_t> unknown;
  for (const auto& [metric_id, index_id] : msg.metric_to_index_id()) {
    if (!_series_conf.labels->get(metric_id))
      unknown.push_back(metric_id);
  }
  // name and unit are read before the deletion, from the series itself
  if (!unknown.empty())
    _resolve_labels(unknown);

  std::vector<std::string> selectors;
  absl::flat_hash_set<uint64_t> indexes;
  for (const auto& [metric_id, index_id] : msg.metric_to_index_id()) {
    selectors.emplace_back(fmt::format("metric_val{{id=\"{}\"}}", metric_id));
    if (index_id && indexes.insert(index_id).second)
      selectors.emplace_back(fmt::format("status_val{{id=\"{}\"}}", index_id));
  }
  _delete_series(selectors);
  for (const auto& [metric_id, index_id] : msg.metric_to_index_id())
    _rebuilt[metric_id] = index_id;
}

void stream::_rebuild_data(const RebuildMessage& msg) {
  auto cache = cache::global_cache::instance_ptr();
  if (!cache)
    throw msg_fmt("victoria_metrics: no global cache to rebuild metrics");

  struct index_status {
    uint64_t host_id;
    uint64_t service_id;
    std::map<int64_t, uint32_t> time_to_state;
  };
  std::map<uint64_t, index_status> statuses;

  http_tsdb::request::pointer req = create_request();
  auto send_full = [&](bool force) {
    if (req->get_nb_data() &&
        (force ||
         req->get_nb_data() >= _conf->get_max_queries_per_transaction())) {
      send_sync(req);
      req = create_request();
    }
  };

  for (const auto& [metric_id, serie] : msg.timeserie()) {
    if (serie.pts().empty())
      continue;
    uint64_t index_id;
    auto rebuilt = _rebuilt.find(metric_id);
    if (rebuilt != _rebuilt.end() && rebuilt->second)
      index_id = rebuilt->second;
    else
      index_id = cache->get_index_id_from_metric_id(metric_id);
    std::optional<cache::host_serv_pair> host_serv;
    if (index_id)
      host_serv = cache->get_host_serv_id(index_id);
    if (!host_serv) {
      SPDLOG_LOGGER_ERROR(_logger,
                          "victoria_metrics: metric {} not rebuilt, its index "
                          "{} or service is unknown",
                          metric_id, index_id);
      continue;
    }
    std::optional<metric_labels::labels> labels =
        _series_conf.labels->get(metric_id);
    if (!labels)
      SPDLOG_LOGGER_ERROR(_logger,
                          "victoria_metrics: name and unit of metric {} "
                          "unknown, rebuilt without them",
                          metric_id);

    storage::pb_metric point;
    Metric& obj = point.mut_obj();
    obj.set_metric_id(metric_id);
    obj.set_host_id(host_serv->first);
    obj.set_service_id(host_serv->second);
    if (labels) {
      obj.set_name(labels->name);
      obj.set_unit(labels->unit);
    }
    index_status& status = statuses[index_id];
    status.host_id = host_serv->first;
    status.service_id = host_serv->second;
    for (const Point& pt : serie.pts()) {
      obj.set_time(pt.ctime());
      obj.set_value(pt.value());
      req->add_metric(point);
      // UNKNOWN is a gap, as for the live status
      if (pt.status() <= 2)
        status.time_to_state[pt.ctime()] = pt.status();
      send_full(false);
    }
    SPDLOG_LOGGER_DEBUG(_logger, "victoria_metrics: {} points of metric {}",
                        serie.pts().size(), metric_id);
  }

  for (const auto& [index_id, status] : statuses) {
    storage::pb_status point;
    Status& obj = point.mut_obj();
    obj.set_index_id(index_id);
    obj.set_host_id(status.host_id);
    obj.set_service_id(status.service_id);
    for (const auto& [ctime, state] : status.time_to_state) {
      obj.set_time(ctime);
      obj.set_state(state);
      req->add_status(point);
      send_full(false);
    }
  }
  send_full(true);
}

void stream::_rebuild_end(const RebuildMessage& msg) {
  SPDLOG_LOGGER_INFO(_logger,
                     "victoria_metrics: end of the rebuild of metrics ({})",
                     absl::StrJoin(msg.metric_to_index_id(), ",",
                                   absl::PairFormatter(":")));
  drain_requests();
  for (const auto& [metric_id, index_id] : msg.metric_to_index_id())
    _rebuilt.erase(metric_id);
}
