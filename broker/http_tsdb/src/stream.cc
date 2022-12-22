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

using namespace com::centreon::broker;
using namespace com::centreon::broker::http_tsdb;

void request::append(const request::pointer& data_to_append) {
  body() += data_to_append->body();
  _nb_metric += data_to_append->_nb_metric;
  _nb_status += data_to_append->_nb_status;
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
      _acknowledged(0),
      _timeout_send_timer(*io_context) {
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
  to_wait.wait();
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

void stream::statistics(nlohmann::json& tree) const {}

int stream::write(std::shared_ptr<io::data> const& data) {
  // Take this event into account.
  if (!validate(data, get_name()))
    return 0;

  // Give data to cache.
  _cache.write(data);

  request::pointer to_send;
  unsigned acknowledged = 0;
  {
    std::lock_guard<std::mutex> l(_protect);
    // Process metric events.
    if (data->type() == storage::metric::static_type()) {
      if (!_request) {
        _request = create_request();
      }
      _request->add_metric(_conf, data);
    } else if (data->type() == storage::status::static_type()) {
      if (!_request) {
        _request = create_request();
      }
      _request->add_status(_conf, data);
    }
    if (_request &&
        _request->get_nb_data() >= _conf->get_max_queries_per_transaction()) {
      to_send.swap(_request);
    }
    if (_acknowledged) {
      acknowledged = _acknowledged;
      _acknowledged = 0;
    }
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
  _http_client->send(request, [me = shared_from_this(), request](
                                  const boost::beast::error_code& err,
                                  const std::string& detail,
                                  const http_client::response_ptr& response) {
    me->send_handler(err, detail, request, response);
  });
}

void stream::send_request(const request::pointer& request,
                          const std::shared_ptr<std::promise<void>>& prom) {
  _http_client->send(
      request,
      [me = shared_from_this(), request, prom](
          const boost::beast::error_code& err, const std::string& detail,
          const http_client::response_ptr& response) mutable {
        me->send_handler(err, detail, request, response);
        prom->set_value();
      });
}

void stream::send_handler(const boost::beast::error_code& err,
                          const std::string& detail,
                          const request::pointer& request,
                          const http_client::response_ptr& response) {
  if (err) {
    SPDLOG_LOGGER_ERROR(_logger, "fail to send {} events to database: {} , {}",
                        request->get_nb_data(), err.message(), detail);
    std::lock_guard<std::mutex> l(_protect);
    if (_request) {
      request->append(_request);
    }
    _request = request;
  } else {
    SPDLOG_LOGGER_DEBUG(_logger, "{} metrics and {} status sent to database",
                        request->get_nb_metric(), request->get_nb_status());
    std::lock_guard<std::mutex> l(_protect);
    _acknowledged += request->get_nb_data();
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
