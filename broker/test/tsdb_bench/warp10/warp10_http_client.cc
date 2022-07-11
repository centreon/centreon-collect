#include "warp10_http_client.hh"

using namespace warp10;

warp10_client::warp10_client(const io_context_ptr& io_context,
                             const logger_ptr& logger,
                             const boost::json::object& conf)
    : db_conf(conf, logger), _logger(logger) {
  const boost::json::value* val = conf.if_contains("read_token");
  if (val) {
    _read_token = val->as_string().c_str();
  }

  boost::asio::ip::tcp::resolver res(*io_context);
  boost::asio::ip::tcp::resolver::results_type result;
  try {
    result = std::move(res.resolve(_host, std::to_string(_port)));
  } catch (const boost::exception& e) {
    SPDLOG_LOGGER_ERROR(logger, "fail to resolve {}:{}  {}", _host, _port,
                        boost::diagnostic_information(e));
    throw;
  }

  _conn = std::make_shared<http_client::connection>(io_context, logger,
                                                    *result.begin());
}

warp10_client::request_ptr warp10_client::create_request(unsigned nb_metrics) {
  request_ptr ret = std::make_shared<http_client::connection::request_type>();
  ret->set(boost::beast::http::field::host, _host);
  ret->set(boost::beast::http::field::content_type, "text/plain");
  ret->set(boost::beast::http::field::accept, "application/json");
  // we save token returned by influxdb in _password
  ret->set("X-Warp10-Token", _password);
  ret->body().reserve(150 * nb_metrics);
  ret->keep_alive(true);
  ret->method(boost::beast::http::verb::post);
  ret->target("/api/v0/update");
  SPDLOG_LOGGER_DEBUG(_logger, "create {} metrics request", nb_metrics);

  return ret;
}

void warp10_client::on_receive(
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

void warp10_client::select(
    const time_point& begin,
    const time_point& end,
    uint64_t metric_id,
    unsigned nb_point,
    http_client::connection::send_callback_type callback) {
  request_ptr request =
      std::make_shared<http_client::connection::request_type>();
  request->set(boost::beast::http::field::host, _host);
  request->set("X-Warp10-Token", _read_token);
  request->set(boost::beast::http::field::content_type, "text/plain");
  request->set(boost::beast::http::field::accept, "application/json");
  request->method(boost::beast::http::verb::post);
  request->body() = fmt::format(
      "[\n '{}'\n 'metric{}'\n {{}}\n '{}Z'\n '{}Z'\n] FETCH\n[\n SWAP\n"
      " bucketizer.mean 0 0 {} \n] BUCKETIZE",
      _read_token, metric_id, date::format("%FT%T", begin),
      date::format("%FT%T", end), nb_point);

  request->target("/api/v0/exec");
  request->content_length(request->body().length());

  time_point before_send_request = system_clock::now();
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
