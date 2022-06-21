#include "warp10_http_client.hh"

using namespace warp10;

warp10_client::warp10_client(const io_context_ptr& io_context,
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
