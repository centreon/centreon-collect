#ifndef __INFLUXDB__HTTP__CLIENT_HH__
#define __INFLUXDB__HTTP__CLIENT_HH__

#include "db_conf.hh"
#include "http_connection.hh"

namespace influxdb {
class inf_client : public std::enable_shared_from_this<inf_client>,
                   public db_conf {
  http_client::connection::pointer _conn;
  logger_ptr _logger;
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
  using pointer = std::shared_ptr<inf_client>;

  inf_client(const io_context_ptr& io_context,
             const logger_ptr& logger,
             const boost::json::object& conf);

  void connect(http_client::connection::connect_callback_type callback) {
    _conn->connect(callback);
  }

  template <class metric_iter>
  void send(metric_iter begin,
            metric_iter end,
            http_client::connection::send_callback_type callback);
};

template <class metric_iter>
inf_client::request_ptr inf_client::create_request(metric_iter begin,
                                                   metric_iter end) {
  request_ptr req = create_request(std::distance(begin, end));

  std::string& body = req->body();
  for (; begin != end; ++begin) {
    body.append("metric");
    absl::StrAppend(&body, begin->get_metric_id());
    body.append(",host=");
    absl::StrAppend(&body, begin->get_host_id());
    body.append(",serv=");
    absl::StrAppend(&body, begin->get_service_id());
    body.append(" val=");
    begin->get_value(body);
    body.push_back(' ');
    absl::StrAppend(&body,
                    std::chrono::duration_cast<std::chrono::milliseconds>(
                        begin->get_time().time_since_epoch())
                        .count());
    body.push_back('\n');
  }
  body.push_back('\n');
  req->set(boost::beast::http::field::content_length,
           std::to_string(body.length()));
  return req;
}

template <class metric_iter>
void inf_client::send(metric_iter begin,
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

}  // namespace influxdb

#endif
