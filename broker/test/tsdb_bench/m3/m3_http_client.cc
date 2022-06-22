
#include <snappy.h>
#include "remote.pb.h"

#include "m3_http_client.hh"

using namespace m3db;

namespace m3db {
namespace detail {
class double_converter : public boost::static_visitor<double> {
 public:
  template <typename T>
  double operator()(const T& val) const {
    return double(val);
  }
};
}  // namespace detail
}  // namespace m3db

m3_client::m3_client(const io_context_ptr& io_context,
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

m3_client::request_ptr m3_client::create_request(unsigned nb_metrics) {
  request_ptr ret = std::make_shared<http_client::connection::request_type>();
  ret->set(boost::beast::http::field::host, _host);
  ret->set(boost::beast::http::field::content_type, "text/plain");
  ret->set(boost::beast::http::field::accept, "application/octet-stream");
  // we save token returned by influxdb in _password
  ret->set(boost::beast::http::field::authorization, "Token " + _password);
  ret->keep_alive(true);
  ret->method(boost::beast::http::verb::post);
  ret->target("/api/v1/prom/remote/write");
  SPDLOG_LOGGER_DEBUG(_logger, "create {} metrics request", nb_metrics);

  return ret;
}

void m3_client::on_receive(
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

void m3_client::send(metric::metric_cont::const_iterator begin,
                     metric::metric_cont::const_iterator end,
                     http_client::connection::send_callback_type callback) {
  request_ptr request = create_request(begin, end);
  _conn->send(request,
              [me = shared_from_this(), request, callback](
                  const boost::beast::error_code& err,
                  const std::string& detail, const response_ptr& response) {
                me->on_receive(err, detail, request, response, callback);
              });
}

m3_client::request_ptr m3_client::create_request(
    metric::metric_cont::const_iterator begin,
    metric::metric_cont::const_iterator end) {
  request_ptr req = create_request(std::distance(begin, end));

  std::string& body = req->body();

  // we have to group metrics by id
  using metric_id_to_metric_map =
      std::map<uint64_t, std::vector<metric::metric_cont::const_iterator>>;

  metric_id_to_metric_map metrics;
  for (; begin != end; ++begin) {
    metrics[begin->get_metric_id()].push_back(begin);
  }

  prometheus::WriteRequest proto_request;

  for (const metric_id_to_metric_map::value_type& metric : metrics) {
    prometheus::TimeSeries* metric_ts = proto_request.add_timeseries();
    prometheus::Label* label = metric_ts->add_labels();
    label->set_name("metric");
    label->set_value(std::to_string(metric.first));
    label = metric_ts->add_labels();
    label->set_name("host");
    label->set_value(std::to_string((*metric.second.begin())->get_host_id()));
    label = metric_ts->add_labels();
    label->set_name("serv");
    label->set_value(
        std::to_string((*metric.second.begin())->get_service_id()));

    // m3 don't accept samples older than 10 minutes or newer than 2 minutes
    time_point now = system_clock::now();
    time_point low_limit = now - std::chrono::seconds(570);
    time_point high_limit = now + std::chrono::seconds(90);

    for (const auto& value : metric.second) {
      prometheus::Sample* sample = metric_ts->add_samples();
      if (value->get_time() < low_limit) {
        sample->set_timestamp(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                low_limit.time_since_epoch())
                .count());
      } else if (value->get_time() > high_limit) {
        sample->set_timestamp(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                high_limit.time_since_epoch())
                .count());
      } else {
        sample->set_timestamp(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                value->get_time().time_since_epoch())
                .count());
      }
      sample->set_value(
          boost::apply_visitor(detail::double_converter(), value->get_value()));
    }
  }

  std::string uncompressed;
  proto_request.AppendToString(&uncompressed);

  snappy::Compress(uncompressed.data(), uncompressed.length(), &body);
  req->set(boost::beast::http::field::content_length,
           std::to_string(body.length()));
  return req;
}
