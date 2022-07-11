#include "inf_http_client.hh"

using namespace influxdb;

inf_client::inf_client(const io_context_ptr& io_context,
                       const logger_ptr& logger,
                       const boost::json::object& conf)
    : db_conf(conf, logger), _logger(logger) {
  boost::asio::ip::tcp::resolver res(*io_context);
  boost::asio::ip::tcp::resolver::results_type result;
  try {
    result = std::move(res.resolve(_host, std::to_string(_port)));
  } catch (const boost::exception& e) {
    SPDLOG_LOGGER_ERROR(logger, "fail to resolve {}:{}  {}", _host, _port,
                        boost::diagnostic_information(e));
    throw;
  }

  const boost::json::value* val = conf.if_contains("organization_id");
  if (val) {
    _organization_id = val->as_string().c_str();
  }
  _conn = std::make_shared<http_client::connection>(io_context, logger,
                                                    *result.begin());
}

inf_client::request_ptr inf_client::create_request(unsigned nb_metrics) {
  request_ptr ret = std::make_shared<http_client::connection::request_type>();
  ret->set(boost::beast::http::field::host, _host);
  ret->set(boost::beast::http::field::content_type, "text/plain");
  ret->set(boost::beast::http::field::accept, "application/json");
  ret->set(boost::beast::http::field::accept_encoding, "gzip");
  // we save token returned by influxdb in _password
  ret->set(boost::beast::http::field::authorization, "Token " + _password);
  ret->body().reserve(100 * nb_metrics);
  ret->keep_alive(true);
  ret->method(boost::beast::http::verb::post);
  if (_conf["database_type"] == "influxdb") {
    ret->target("/api/v2/write?org=" + _name + "&bucket=" + _user +
                "&precision=ms");
  } else {  // victoria metrics
    ret->target("/write");
  }
  SPDLOG_LOGGER_DEBUG(_logger, "create {} metrics request", nb_metrics);

  return ret;
}

void inf_client::on_receive(
    const boost::beast::error_code& err,
    const std::string& detail,
    const request_ptr& request,
    const response_ptr& response,
    http_client::connection::send_callback_type callback) {
  if (err) {
    SPDLOG_LOGGER_ERROR(_logger, "fail to send or recv {}: {}", *request,
                        err.message());
  } else {
    SPDLOG_LOGGER_DEBUG(_logger, "response: {}", *response);
  }
  callback(err, detail, response);
}

void inf_client::select(const time_point& begin,
                        const time_point& end,
                        uint64_t metric_id,
                        unsigned nb_point,
                        http_client::connection::send_callback_type callback) {
  if (_conf["database_type"] == "influxdb") {
    select_influx_db(begin, end, metric_id, nb_point, callback);
  } else {  // victoria metrics
    select_victoria(begin, end, metric_id, nb_point, callback);
  }
}

void inf_client::select_influx_db(
    const time_point& begin,
    const time_point& end,
    uint64_t metric_id,
    unsigned nb_point,
    http_client::connection::send_callback_type callback) {
  request_ptr request =
      std::make_shared<http_client::connection::request_type>();
  request->set(boost::beast::http::field::host, _host);
  request->set(boost::beast::http::field::content_type, "application/vnd.flux");
  request->set(boost::beast::http::field::accept, "application/csv");
  // we save token returned by influxdb in _password
  request->set(boost::beast::http::field::authorization, "Token " + _password);
  request->keep_alive(true);
  request->method(boost::beast::http::verb::post);
  if (_conf["database_type"] == "influxdb") {
    request->target("/api/v2/query?orgID=" + _organization_id);
  } else {  // victoria metrics
    request->target("/write");
  }
  request->body() = fmt::format(
      "from(bucket:\"{}\") |> range(start: {}Z, stop: {}Z) |> filter(fn: "
      "(r) "
      "=> "
      "r._measurement == \"metric{}\")",
      _user, date::format("%FT%T", begin), date::format("%FT%T", end),
      metric_id);

  unsigned nb_second =
      std::chrono::duration_cast<std::chrono::seconds>(end - begin).count();
  if (nb_second / 3600 >= nb_point) {  // less than one point by hour
    request->body() +=
        " |> aggregateWindow(every: 1m, fn: mean, createEmpty : false)";
  } else if (nb_second / 60 >= nb_point) {  // less than one point by minute
    request->body() +=
        " |> aggregateWindow(every: 1h, fn: mean, createEmpty : false)";
  } else {
    request->body() +=
        " |> aggregateWindow(every: 1s, fn: mean, createEmpty : false)";
  }
  request->content_length(request->body().length());

  _conn->send(request,
              [me = shared_from_this(), request, callback](
                  const boost::beast::error_code& err,
                  const std::string& detail, const response_ptr& response) {
                me->on_receive(err, detail, request, response, callback);
              });
}

void inf_client::select_victoria(
    const time_point& begin,
    const time_point& end,
    uint64_t metric_id,
    unsigned nb_point,
    http_client::connection::send_callback_type callback) {
  request_ptr request =
      std::make_shared<http_client::connection::request_type>();
  request->set(boost::beast::http::field::host, _host);
  request->keep_alive(true);
  request->method(boost::beast::http::verb::get);

  unsigned nb_seconds =
      std::chrono::duration_cast<std::chrono::seconds>(end - begin).count();
  unsigned step = nb_seconds / nb_point;
  if (!step) {
    step = 1;
  }

  time_point before_send_request = system_clock::now();

  request->target(fmt::format(
      "/api/v1/query_range?start={}Z&end={}Z&query=metric{}_val&step={}",
      date::format("%FT%T", begin), date::format("%FT%T", end), metric_id,
      step));
  _conn->send(request,
              [me = shared_from_this(), request, callback, before_send_request](
                  const boost::beast::error_code& err,
                  const std::string& detail, const response_ptr& response) {
                if (!err) {
                  std::cout
                      << std::chrono::duration_cast<std::chrono::microseconds>(
                             system_clock::now() - before_send_request)
                             .count();
                }
                me->on_receive(err, detail, request, response, callback);
              });
}
