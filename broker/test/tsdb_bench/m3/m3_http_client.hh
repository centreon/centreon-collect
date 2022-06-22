#ifndef __M3DB__HTTP__CLIENT_HH__
#define __M3DB__HTTP__CLIENT_HH__

#include "db_conf.hh"
#include "http_connection.hh"

namespace m3db {
class m3_client : public std::enable_shared_from_this<m3_client>,
                  public db_conf {
  http_client::connection::pointer _conn;
  logger_ptr _logger;
  using request_ptr = http_client::connection::request_ptr;
  using response_ptr = http_client::connection::response_ptr;

  request_ptr create_request(unsigned nb_metrics);

  request_ptr create_request(metric::metric_cont::const_iterator begin,
                             metric::metric_cont::const_iterator end);

  void on_receive(const boost::beast::error_code& err,
                  const std::string& detail,
                  const request_ptr& request,
                  const response_ptr& response,
                  http_client::connection::send_callback_type callback);

 public:
  using pointer = std::shared_ptr<m3_client>;

  m3_client(const io_context_ptr& io_context,
            const logger_ptr& logger,
            const boost::json::object& conf);

  void connect(http_client::connection::connect_callback_type callback) {
    _conn->connect(callback);
  }

  void send(metric::metric_cont::const_iterator begin,
            metric::metric_cont::const_iterator end,
            http_client::connection::send_callback_type callback);
};

}  // namespace m3db

#endif
