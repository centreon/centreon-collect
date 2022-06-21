#ifndef __TSDB__HTTP_CLIENT_CONNECTION_HH__
#define __TSDB__HTTP_CLIENT_CONNECTION_HH__

#include "metric.hh"

namespace http_client {

/**
 * @brief it represents an http client connection to a server
 * it may be reused if http keep alive is active
 *
 */
class connection : public std::enable_shared_from_this<connection> {
 public:
  using pointer = std::shared_ptr<connection>;

  using request_type =
      boost::beast::http::request<boost::beast::http::string_body>;
  using request_ptr = std::shared_ptr<request_type>;
  using response_type =
      boost::beast::http::response<boost::beast::http::string_body>;
  using response_ptr = std::shared_ptr<response_type>;

  using connect_callback_type =
      std::function<void(const boost::beast::error_code&, const std::string&)>;

  using send_callback_type = std::function<void(const boost::beast::error_code&,
                                                const std::string&,
                                                const response_ptr&)>;

 protected:
  boost::beast::tcp_stream _socket;
  io_context_ptr _io_context;
  logger_ptr _logger;
  boost::asio::ip::tcp::endpoint _endpt;
  boost::beast::flat_buffer _recv_buffer;

  void on_connect(const boost::beast::error_code& err,
                  connect_callback_type callback);

  void on_sent(const boost::beast::error_code& err,
               request_ptr request,
               send_callback_type callback);
  void on_read(const boost::beast::error_code& err,
               const request_ptr& request,
               send_callback_type callback,
               const response_ptr& resp);

 public:
  connection(const io_context_ptr& io_context,
             const logger_ptr& logger,
             boost::asio::ip::tcp::endpoint endpt);

  ~connection();

  void connect(connect_callback_type callback);

  void send(request_ptr request, send_callback_type callback);
};
};  // namespace http_client

#endif
