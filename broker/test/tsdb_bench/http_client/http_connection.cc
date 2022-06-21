#include "http_client/http_connection.hh"

using namespace http_client;

connection::connection(const io_context_ptr& io_context,
                       const logger_ptr& logger,
                       boost::asio::ip::tcp::endpoint endpt)
    : _socket(*io_context),
      _io_context(io_context),
      _logger(logger),
      _endpt(endpt) {
  SPDLOG_LOGGER_DEBUG(_logger, "create connection to {}", _endpt);
}

connection::~connection() {
  SPDLOG_LOGGER_DEBUG(_logger, "delete connection to {}", _endpt);
}

void connection::connect(connect_callback_type callback) {
  SPDLOG_LOGGER_DEBUG(_logger, "connect to {}", _endpt);
  _socket.async_connect(_endpt, [me = shared_from_this(), callback](
                                    const boost::beast::error_code& err) {
    me->on_connect(err, callback);
  });
}

void connection::on_connect(const boost::beast::error_code& err,
                            connect_callback_type callback) {
  if (err) {
    SPDLOG_LOGGER_ERROR(_logger, "fail connect to {}: {}", _endpt,
                        err.message());
    callback(err, fmt::format("fail to connect to {}", _endpt));
    return;
  }
  SPDLOG_LOGGER_DEBUG(_logger, "connected to {}", _endpt);
  callback(err, {});
}

void connection::send(request_ptr request, send_callback_type callback) {
  SPDLOG_LOGGER_DEBUG(_logger, "send request to {}", _endpt);

  boost::beast::http::async_write(
      _socket, *request,
      [me = shared_from_this(), request, callback](
          const boost::beast::error_code& err, size_t bytes_transfered) {
        me->on_sent(err, request, callback);
      });
}

void connection::on_sent(const boost::beast::error_code& err,
                         request_ptr request,
                         send_callback_type callback) {
  if (err) {
    SPDLOG_LOGGER_ERROR(_logger, "fail send {} to {}: {}", *request, _endpt,
                        err.message());
    callback(err, fmt::format("fail send request to ", _endpt), {});
    return;
  }

  SPDLOG_LOGGER_DEBUG(_logger, "request sent to {}", _endpt);

  response_ptr resp = std::make_shared<response_type>();
  boost::beast::http::async_read(
      _socket, _recv_buffer, *resp,
      [me = shared_from_this(), request, callback, resp](
          const boost::beast::error_code& ec, std::size_t) {
        me->on_read(ec, request, callback, resp);
      });
}

void connection::on_read(const boost::beast::error_code& err,
                         const request_ptr& request,
                         send_callback_type callback,
                         const response_ptr& resp) {
  if (err) {
    SPDLOG_LOGGER_ERROR(_logger, "fail receive response to {} from {}: {}",
                        *request, _endpt, err.message());

    callback(err, fmt::format("fail receive from {}", _endpt), resp);
    return;
  }

  SPDLOG_LOGGER_DEBUG(_logger, "recv response from {} {}", _endpt, *resp);

  callback(err, {}, resp);
}
