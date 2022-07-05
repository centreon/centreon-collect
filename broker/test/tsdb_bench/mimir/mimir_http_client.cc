
#include <snappy.h>
#include "mimir.pb.h"

#include "mimir_http_client.hh"

using namespace mimir;

namespace mimir {
namespace detail {
class double_converter : public boost::static_visitor<double> {
 public:
  template <typename T>
  double operator()(const T& val) const {
    return double(val);
  }
};
}  // namespace detail
}  // namespace mimir

mimir_client::mimir_client(const io_context_ptr& io_context,
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

mimir_client::request_ptr mimir_client::create_request(unsigned nb_metrics) {
  request_ptr ret = std::make_shared<http_client::connection::request_type>();
  ret->set(boost::beast::http::field::host, _host);
  ret->set("X-Prometheus-Remote-Write-Version", "0.1.0");
  ret->set(boost::beast::http::field::content_type, "text/plain");
  ret->set(boost::beast::http::field::accept, "application/octet-stream");
  ret->keep_alive(true);
  ret->method(boost::beast::http::verb::post);
  ret->target("/api/v1/push");
  SPDLOG_LOGGER_DEBUG(_logger, "create {} metrics request", nb_metrics);

  return ret;
}

void mimir_client::on_receive(
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

void mimir_client::send(metric::metric_cont::const_iterator begin,
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

mimir_client::request_ptr mimir_client::create_request(
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

  cortexpb::WriteRequest proto_request;
  proto_request.set_source(
      ::cortexpb::WriteRequest_SourceEnum::WriteRequest_SourceEnum_API);

  for (const metric_id_to_metric_map::value_type& metric : metrics) {
    cortexpb::TimeSeries* metric_ts = proto_request.add_timeseries();
    cortexpb::LabelPair* label = metric_ts->add_labels();
    label->set_name("__name__");
    label->set_value("metric" + std::to_string(metric.first));
    label = metric_ts->add_labels();
    label->set_name("host");
    label->set_value(std::to_string((*metric.second.begin())->get_host_id()));
    label = metric_ts->add_labels();
    label->set_name("serv");
    label->set_value(
        std::to_string((*metric.second.begin())->get_service_id()));

    for (const auto& value : metric.second) {
      cortexpb::Sample* sample = metric_ts->add_samples();
      sample->set_timestamp_ms(
          std::chrono::duration_cast<std::chrono::milliseconds>(
              value->get_time().time_since_epoch())
              .count());
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
