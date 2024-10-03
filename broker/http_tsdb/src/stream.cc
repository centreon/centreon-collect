/**
 * Copyright 2024 Centreon (https://www.centreon.com/)
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

#include "com/centreon/broker/http_tsdb/stream.hh"
#include "bbdo/storage/metric.hh"
#include "bbdo/storage/status.hh"
#include "com/centreon/broker/cache/global_cache.hh"
#include "com/centreon/broker/exceptions/shutdown.hh"
#include "com/centreon/broker/http_tsdb/internal.hh"

using namespace com::centreon::broker;
using namespace com::centreon::exceptions;
using namespace com::centreon::broker::http_tsdb;

/************************************************************************
 *      request
 ************************************************************************/

/**
 * @brief when a request fails the pending request is appended to the failed
 * request then stream will retry to send failed metric added of pending metrics
 *
 * @param data_to_append
 */
void request::append(const request::pointer& data_to_append) {
  body() += data_to_append->body();
  _nb_metric += data_to_append->_nb_metric;
  _nb_status += data_to_append->_nb_status;
}

void request::dump(std::ostream& stream) const {
  request_base::dump(stream);
  stream << " nb metric: " << _nb_metric << " nb status:" << _nb_status;
}

/************************************************************************
 *      statistics
 ************************************************************************/

/**
 * @brief when we calculate average statistics, average is calculated over
 * avg_plage ie 1s
 *
 */
static constexpr duration avg_plage(std::chrono::seconds(1));

/**
 * @brief add point to statistic
 * before adding a point, it's erase tool old points
 * @param value
 */
void stream::stat_average::add_point(unsigned value) {
  time_point now(system_clock::now());
  time_point avg_limit(now - avg_plage);
  // little clean of too old points
  while (!_points.empty()) {
    auto begin = _points.begin();
    if (begin->first >= avg_limit) {
      break;
    }
    _points.erase(begin);
  }
  // insert or add
  auto insert_res = _points.emplace(now, value);
  if (!insert_res.second) {
    insert_res.first->second += value;
  }
}

/**
 * @brief calculate current avg
 *
 * @return unsigned
 */
unsigned stream::stat_average::get_average() const {
  unsigned avg = 0;
  if (!_points.empty()) {
    for (const auto& point : _points) {
      avg += point.second;
    }
    avg /= _points.size();
  }
  return avg;
}

/************************************************************************
 *      stream
 ************************************************************************/

/**
 * @brief Construct a new stream::stream object
 * conn_creator is used by this object to construct http::connection_base
 * objects conn_creator can construct an http_connection, https_connection or a
 * mock
 * @param name
 * @param io_context
 * @param logger
 * @param conf
 * @param cache
 * @param conn_creator
 */
stream::stream(const std::string& name,
               const std::shared_ptr<asio::io_context>& io_context,
               const std::shared_ptr<spdlog::logger>& logger,
               const std::shared_ptr<http_tsdb_config>& conf,
               http::connection_creator conn_creator)
    : io::stream(name),
      _io_context(io_context),
      _logger(logger),
      _conf(conf),
      _acknowledged(0),
      _success_request_stat{{0, 0}, {0, 0}},
      _failed_request_stat{{0, 0}, {0, 0}},
      _metric_stat{{0, 0}, {0, 0}},
      _status_stat{{0, 0}, {0, 0}} {
  _http_client =
      common::http::client::load(io_context, logger, conf, conn_creator);
}

stream::~stream() {}

/**
 * @brief this method is the only one blocking method
 * it sends all pending data to the server and wait completion for one second
 *
 * @return int number of metric and status sent
 */
int stream::flush() {
  request::pointer to_send;
  {
    std::lock_guard<std::mutex> l(_protect);
    if (_request) {
      to_send.swap(_request);
      _request.reset();

    } else {
      unsigned acknowledged = _acknowledged;
      _acknowledged = 0;
      return acknowledged;
    }
  }
  SPDLOG_LOGGER_DEBUG(_logger, "{} flush {} event", get_name(),
                      to_send->get_nb_data());
  send_request(to_send);
  std::lock_guard<std::mutex> l(_protect);
  unsigned acknowledged = _acknowledged;
  _acknowledged = 0;
  return acknowledged;
}

/**
 * @brief this output stream don't perform any read operation
 * @throw
 * @param d
 * @return true
 * @return false
 */
bool stream::read(std::shared_ptr<io::data>& d, time_t) {
  d.reset();
  throw exceptions::shutdown(
      fmt::format("cannot read from {} database", get_name()));
}

/**
 * @brief return statistics of the stream like nulber of request, send and
 * receive durations it writes stats in tree
 *
 * @param tree
 */
void stream::statistics(nlohmann::json& tree) const {
  auto extract_stat = [&](const char* label, const stat_average& data) {
    if (!data.empty()) {
      tree[label] = data.get_average();
    }
  };

  std::lock_guard<std::mutex> l(_protect);
  tree["success_request"] = _success_request_stat[0].value;
  tree["failed_request"] = _failed_request_stat[0].value;
  tree["metric_sent"] = _metric_stat[0].value;
  tree["status_sent"] = _status_stat[0].value;

  extract_stat("avg_connect_ms", _connect_avg);
  extract_stat("avg_send_ms", _send_avg);
  extract_stat("avg_connect_ms", _recv_avg);
}

/**
 * @brief push metric or status to the tsdb
 * it process these four events:
 *  - storage::metric
 *  - storage::status
 *  - storage::pb_metric
 *  - storage::pb_status
 *  .
 * other metrics are only used by cache
 *
 * metric or status is not sent right now, there are buffered and sent if number
 * of pending metrics exceed conf._max_queries_per_transaction or last sent is
 * older than conf._max_send_interval
 *
 * @param data
 * @return int
 */
int stream::write(std::shared_ptr<io::data> const& data) {
  // Take this event into account.
  unsigned acknowledged = 0;
  if (!validate(data, get_name())) {
    std::lock_guard<std::mutex> l(_protect);
    ++_acknowledged;
    return 0;
  }

  request::pointer to_send;
  {
    std::lock_guard<std::mutex> l(_protect);
    // Process metric events.
    switch (data->type()) {
      case storage::metric::static_type(): {
        if (!_request) {
          _request = create_request();
        }
        SPDLOG_LOGGER_TRACE(_logger, "add metric: {}", *data);
        Metric converted;
        std::static_pointer_cast<storage::metric>(data)->convert_to_pb(
            converted);
        _request->add_metric(converted);
        break;
      }
      case storage::pb_metric::static_type():
        if (!_request) {
          _request = create_request();
        }
        SPDLOG_LOGGER_TRACE(_logger, "add metric: {}", *data);
        _request->add_metric(
            std::static_pointer_cast<storage::pb_metric>(data)->obj());
        break;
      case storage::status::static_type(): {
        if (!_request) {
          _request = create_request();
        }
        SPDLOG_LOGGER_TRACE(_logger, "add status: {}", *data);
        Status converted;
        std::static_pointer_cast<storage::status>(data)->convert_to_pb(
            converted);
        {
          const cache::host_serv_pair* host_serv =
              cache::global_cache::instance_ptr()->get_host_serv_id(
                  converted.index_id());
          if (!host_serv) {
            SPDLOG_LOGGER_ERROR(
                _logger, "unable to find host_id service_id from index_id:{}",
                converted.index_id());
          } else {
            converted.set_host_id(host_serv->first);
            converted.set_service_id(host_serv->second);
          }
        }
        if (converted.service_id()) {
          _request->add_status(converted);
        }
        break;
      }
      case storage::pb_status::static_type():
        if (!_request) {
          _request = create_request();
        }
        SPDLOG_LOGGER_TRACE(_logger, "add status: {}", *data);
        _request->add_status(
            std::static_pointer_cast<storage::pb_status>(data)->obj());
        break;
      default:
        ++_acknowledged;
        break;
    }
    // enought metrics to send?
    if (_request &&
        _request->get_nb_data() >= _conf->get_max_queries_per_transaction()) {
      to_send.swap(_request);
    }
    acknowledged = _acknowledged;
    _acknowledged = 0;
  }
  if (to_send) {  // if enought metrics to send => send
    send_request(to_send);
  }
  return acknowledged;
}

int32_t stream::stop() {
  int32_t retval = flush();
  _http_client->shutdown();
  SPDLOG_LOGGER_INFO(_logger, "{} stream stopped with {} acknowledged events",
                     get_name(), retval);
  return retval;
}

/**
 * @brief send request to tsdb
 * it only calculates content-length of the request before sending it
 *
 * @param request
 */
void stream::send_request(const request::pointer& request) {
  request->content_length(request->body().length());
  _http_client->send(request, [me = shared_from_this(), request](
                                  const boost::beast::error_code& err,
                                  const std::string& detail,
                                  const common::http::response_ptr& response) {
    me->send_handler(err, detail, request, response);
  });
}

static time_point _epoch = system_clock::from_time_t(0);

/**
 * @brief send handler called by http_client object
 * if err is set, current request is appended to request and request is retried
 * in the _timeout_send_timer handler
 *
 * @param err
 * @param detail
 * @param request
 * @param response
 */
void stream::send_handler(const boost::beast::error_code& err,
                          const std::string& detail,
                          const request::pointer& request,
                          const common::http::response_ptr& response
                          [[maybe_unused]]) {
  auto actu_stat_avg = [&]() -> void {
    if (request->get_connect_time() > _epoch &&
        request->get_send_time() > _epoch) {
      _connect_avg.add_point(
          std::chrono::duration_cast<std::chrono::milliseconds>(
              request->get_send_time() - request->get_connect_time())
              .count());
    }
    if (request->get_sent_time() > _epoch &&
        request->get_send_time() > _epoch) {
      _send_avg.add_point(
          std::chrono::duration_cast<std::chrono::milliseconds>(
              request->get_sent_time() - request->get_send_time())
              .count());
    }
    if (request->get_receive_time() > _epoch &&
        request->get_send_time() > _epoch) {
      _recv_avg.add_point(
          std::chrono::duration_cast<std::chrono::milliseconds>(
              request->get_receive_time() - request->get_sent_time())
              .count());
    }
  };
  if (err) {
    SPDLOG_LOGGER_ERROR(_logger, "fail to send {} events to database: {} , {}",
                        request->get_nb_data(), err.message(), detail);
    std::lock_guard<std::mutex> l(_protect);
    add_to_stat(_failed_request_stat, 1);
    actu_stat_avg();
    if (_request) {  // we musn't lost any data
      request->append(_request);
    }
    _request = request;
  } else {
    SPDLOG_LOGGER_DEBUG(_logger, "{} metrics and {} status sent to database",
                        request->get_nb_metric(), request->get_nb_status());
    std::lock_guard<std::mutex> l(_protect);
    add_to_stat(_success_request_stat, 1);
    add_to_stat(_metric_stat, request->get_nb_metric());
    add_to_stat(_status_stat, request->get_nb_status());
    actu_stat_avg();
    _acknowledged += request->get_nb_data();
  }
}

/**
 * @brief add a point to a cumulated stat
 * if time_point of the stat is not now, time_point and is value is moved to the
 * first element of the array otherwise, point value is added to the current
 *
 * @param to_maj
 * @param to_add
 */
void stream::add_to_stat(stat& to_maj, unsigned to_add) {
  time_t now = time(nullptr);
  if (to_maj[1].time != now) {
    to_maj[0] = to_maj[1];
    to_maj[1] = stat_unit{now, to_add};
  } else {
    to_maj[1].value += to_add;
  }
}
