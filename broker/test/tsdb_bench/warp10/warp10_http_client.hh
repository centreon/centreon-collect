#ifndef __WARP10__HTTP__CLIENT_HH__
#define __WARP10__HTTP__CLIENT_HH__

#include "db_conf.hh"
#include "http_connection.hh"

namespace warp10 {
class warp10_client : public std::enable_shared_from_this<warp10_client>,
                      public db_conf {
  http_client::connection::pointer _conn;
  logger_ptr _logger;
  std::string _read_token;  // write token is stored in db_conf::_password
  using request_ptr = http_client::connection::request_ptr;
  using response_ptr = http_client::connection::response_ptr;

  request_ptr create_request(unsigned nb_metrics);

  template <class metric_iter>
  request_ptr create_request(metric_iter begin, metric_iter end);

  void on_receive(const boost::beast::error_code& err,
                  const std::string& detail,
                  const request_ptr& request,
                  const response_ptr& response,
                  http_client::connection::send_callback_type callback);

 public:
  using pointer = std::shared_ptr<warp10_client>;

  warp10_client(const io_context_ptr& io_context,
                const logger_ptr& logger,
                const boost::json::object& conf);

  void connect(http_client::connection::connect_callback_type callback) {
    _conn->connect(callback);
  }

  template <class metric_iter>
  void send(metric_iter begin,
            metric_iter end,
            http_client::connection::send_callback_type callback);

  void select(const time_point& begin,
              const time_point& end,
              uint64_t metric_id,
              unsigned nb_point,
              http_client::connection::send_callback_type callback);
};

template <class metric_iter>
warp10_client::request_ptr warp10_client::create_request(metric_iter begin,
                                                         metric_iter end) {
  request_ptr req = create_request(std::distance(begin, end));

  std::string& body = req->body();
  for (; begin != end; ++begin) {
    absl::StrAppend(&body,
                    std::chrono::duration_cast<std::chrono::microseconds>(
                        begin->get_time().time_since_epoch())
                        .count());
    body.append("// ");
    body.append("metric");
    absl::StrAppend(&body, begin->get_metric_id());
    body.append("{host=");
    absl::StrAppend(&body, begin->get_host_id());
    body.append(",serv=");
    absl::StrAppend(&body, begin->get_service_id());
    body.append("} ");
    begin->get_value(body);
    body.push_back('\n');
  }
  body.push_back('\n');
  req->set(boost::beast::http::field::content_length,
           std::to_string(body.length()));
  return req;
}

template <class metric_iter>
void warp10_client::send(metric_iter begin,
                         metric_iter end,
                         http_client::connection::send_callback_type callback) {
  request_ptr request = create_request(begin, end);
  _conn->send(request,
              [me = shared_from_this(), request, callback](
                  const boost::beast::error_code& err,
                  const std::string& detail, const response_ptr& response) {
                me->on_receive(err, detail, request, response, callback);
              });
}

}  // namespace warp10

#endif
