/*
** Copyright 2022 Centreon
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

#include "com/centreon/broker/http_tsdb/stream.hh"
#include "bbdo/storage/metric.hh"
#include "bbdo/storage/status.hh"
#include "com/centreon/broker/exceptions/shutdown.hh"
#include "com/centreon/broker/http_tsdb/internal.hh"
#include "com/centreon/exceptions/msg_fmt.hh"

using namespace com::centreon::broker;
using namespace com::centreon::exceptions;
using namespace com::centreon::broker::http_tsdb;

void request::append(const request::pointer& data_to_append) {
  body() += data_to_append->body();
  _nb_metric += data_to_append->_nb_metric;
  _nb_status += data_to_append->_nb_status;
}

static constexpr duration avg_plage(std::chrono::seconds(1));

void stream::stat_average::add_point(unsigned value) {
  time_point now(system_clock::now());
  time_point avg_limit(now - avg_plage);
  // little clean
  while (!_points.empty()) {
    auto begin = _points.begin();
    if (begin->first >= avg_limit) {
      break;
    }
    _points.erase(begin);
  }
  auto insert_res = _points.emplace(now, value);
  if (!insert_res.second) {
    insert_res.first->second += value;
  }
}

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

stream::stream(const std::string& name,
               const std::shared_ptr<asio::io_context>& io_context,
               const std::shared_ptr<spdlog::logger>& logger,
               const std::shared_ptr<http_tsdb_config>& conf,
               const std::shared_ptr<persistent_cache>& cache,
               http_client::client::connection_creator conn_creator)
    : io::stream(name),
      _io_context(io_context),
      _logger(logger),
      _conf(conf),
      _cache(cache),
      _pending(0),
      _acknowledged(0),
      _timeout_send_timer(*io_context),
      _success_request_stat{{0, 0}, {0, 0}},
      _failed_request_stat{{0, 0}, {0, 0}},
      _metric_stat{{0, 0}, {0, 0}},
      _status_stat{{0, 0}, {0, 0}} {
  _http_client =
      http_client::client::load(io_context, logger, conf, conn_creator);
}

stream::~stream() {}

int stream::flush() {
  request::pointer to_send;
  auto sent_prom = std::make_shared<std::promise<void>>();
  std::future<void> to_wait = sent_prom->get_future();
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
  send_request(to_send, sent_prom);
  to_wait.wait_for(std::chrono::seconds(1));
  std::lock_guard<std::mutex> l(_protect);
  unsigned acknowledged = _acknowledged;
  _acknowledged = 0;
  return acknowledged;
}

bool stream::read(std::shared_ptr<io::data>& d, time_t) {
  d.reset();
  throw exceptions::shutdown(
      fmt::format("cannot read from {} database", get_name()));
}

void stream::statistics(nlohmann::json& tree) const {
  time_t now = time(nullptr);

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

int stream::write(std::shared_ptr<io::data> const& data) {
  // Take this event into account.
  unsigned acknowledged = 0;
  if (!validate(data, get_name())) {
    std::lock_guard<std::mutex> l(_protect);
    acknowledged = _acknowledged + 1;
    _acknowledged = 0;
    return acknowledged;
  }
  // Give data to cache.
  _cache.write(data);

  request::pointer to_send;
  {
    std::lock_guard<std::mutex> l(_protect);
    // Process metric events.
    switch (data->type()) {
      case storage::metric::static_type():
        if (!_request) {
          _request = create_request();
        }
        _request->add_metric(*std::static_pointer_cast<storage::metric>(data));
        ++_pending;
        break;
      case storage::pb_metric::static_type():
        if (!_request) {
          _request = create_request();
        }
        _request->add_metric(
            std::static_pointer_cast<storage::pb_metric>(data)->obj());
        ++_pending;
        break;
      case storage::status::static_type():
        if (!_request) {
          _request = create_request();
        }
        _request->add_status(*std::static_pointer_cast<storage::status>(data));
        ++_pending;
        break;
      case storage::pb_status::static_type():
        if (!_request) {
          _request = create_request();
        }
        _request->add_status(
            std::static_pointer_cast<storage::pb_status>(data)->obj());
        ++_pending;
        break;
      default:
        ++_acknowledged;
        break;
    }
    if (_request &&
        _request->get_nb_data() >= _conf->get_max_queries_per_transaction()) {
      to_send.swap(_request);
    }
    acknowledged = _acknowledged;
    _acknowledged = 0;
  }
  if (to_send) {
    send_request(to_send);
  } else {
    start_timeout_send_timer();
  }
  return acknowledged;
}

int32_t stream::stop() {
  int32_t retval = flush();
  SPDLOG_LOGGER_INFO(_logger, "{} stream stopped with {} acknowledged events",
                     get_name(), retval);
  return retval;
}

void stream::send_request(const request::pointer& request) {
  request->content_length(request->body().length());
  _http_client->send(request, [me = shared_from_this(), request](
                                  const boost::beast::error_code& err,
                                  const std::string& detail,
                                  const http_client::response_ptr& response) {
    me->send_handler(err, detail, request, response);
  });
}

void stream::send_request(const request::pointer& request,
                          const std::shared_ptr<std::promise<void>>& prom) {
  request->content_length(request->body().length());
  _http_client->send(
      request,
      [me = shared_from_this(), request, prom](
          const boost::beast::error_code& err, const std::string& detail,
          const http_client::response_ptr& response) mutable {
        me->send_handler(err, detail, request, response);
        prom->set_value();
      });
}

static time_point _epoch = system_clock::from_time_t(0);

void stream::send_handler(const boost::beast::error_code& err,
                          const std::string& detail,
                          const request::pointer& request,
                          const http_client::response_ptr& response) {
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
    if (_request) {
      request->append(_request);
    }
    _request = request;
    start_timeout_send_timer_no_lock();
  } else {
    SPDLOG_LOGGER_DEBUG(_logger, "{} metrics and {} status sent to database",
                        request->get_nb_metric(), request->get_nb_status());
    std::lock_guard<std::mutex> l(_protect);
    add_to_stat(_success_request_stat, 1);
    add_to_stat(_metric_stat, request->get_nb_metric());
    add_to_stat(_status_stat, request->get_nb_status());
    actu_stat_avg();
    _acknowledged += request->get_nb_data();
    if (request->get_nb_data() <= _pending) {
      _pending -= request->get_nb_data();
    } else {
      _pending = 0;
    }
  }
}

void stream::start_timeout_send_timer() {
  bool expected = false;
  if (_timeout_send_timer_run.compare_exchange_strong(expected, true)) {
    std::lock_guard<std::mutex> l(_protect);
    _timeout_send_timer.expires_after(_conf->get_max_send_interval());
    _timeout_send_timer.async_wait(
        [me = shared_from_this()](const boost::system::error_code& err) {
          me->timeout_send_timer_handler(err);
        });
  }
}

void stream::start_timeout_send_timer_no_lock() {
  bool expected = false;
  if (_timeout_send_timer_run.compare_exchange_strong(expected, true)) {
    _timeout_send_timer.expires_after(_conf->get_max_send_interval());
    _timeout_send_timer.async_wait(
        [me = shared_from_this()](const boost::system::error_code& err) {
          me->timeout_send_timer_handler(err);
        });
  }
}

void stream::timeout_send_timer_handler(const boost::system::error_code& err) {
  if (err) {
    return;
  }
  request::pointer to_send;
  {
    std::lock_guard<std::mutex> l(_protect);
    if (_request) {
      to_send.swap(_request);
    }
    _timeout_send_timer_run = false;
  }
  if (to_send) {
    send_request(to_send);
  }
}

void stream::add_to_stat(stat& to_maj, unsigned to_add) {
  time_t now = time(nullptr);
  if (to_maj[1].time != now) {
    to_maj[0] = to_maj[1];
    to_maj[1] = stat_unit{now, to_add};
  } else {
    to_maj[1].value += to_add;
  }
}
