#include "pr_http_server.hh"

using namespace prometheus;

namespace prometheus {
namespace detail {
/******************************************************************************************
 *
 *          accepted http_socket
 *
 ******************************************************************************************/

class http_socket : public std::enable_shared_from_this<http_socket> {
  beast_stream_ptr _sock;
  endpoint_ptr _endpoint;
  http_server::pointer _parent;
  boost::beast::flat_buffer _recv_buffer;

  using response_type =
      boost::beast::http::response<boost::beast::http::string_body>;
  using response_ptr = std::shared_ptr<response_type>;

  using request_type =
      boost::beast::http::request<boost::beast::http::string_body>;
  using request_ptr = std::shared_ptr<request_type>;

  struct value_to_string_visitor : public boost::static_visitor<> {
    std::string* to_append;
    value_to_string_visitor(std::string* to_app) : to_append(to_app) {}

    template <typename T>
    void operator()(const T& value) const {
      absl::StrAppend(to_append, value);
    }
  };

  http_socket(const beast_stream_ptr& socket,
              const endpoint_ptr& endpoint,
              const http_server::pointer parent);

  void start_read();
  void on_read(const boost::beast::error_code& ec, const request_ptr& request);

  response_ptr create_response(const request_ptr& request);
  void answer(const request_ptr& request);
  void close();

  class answer_builder {
    std::string& _response_body;

   public:
    answer_builder(std::string& response_body)
        : _response_body(response_body) {}
    void operator()(const metric& met);
  };

  void on_write(const boost::beast::error_code& err,
                size_t bytes_transfered,
                const request_ptr& request,
                const response_ptr& response);

 public:
  using pointer = std::shared_ptr<http_socket>;

  ~http_socket();

  static void create_and_start(const beast_stream_ptr& socket,
                               const endpoint_ptr& endpoint,
                               const http_server::pointer parent);

  endpoint_ptr get_endpoint() const { return _endpoint; }
};

std::ostream& operator<<(std::ostream& stream, const http_socket& to_dump) {
  stream << "this:" << static_cast<const void*>(&to_dump)
         << " dest:" << *to_dump.get_endpoint();

  return stream;
}

void http_socket::answer_builder::operator()(const metric& met) {
  _response_body.append("centreon_metric_");
  absl::StrAppend(&_response_body, met.get_metric_id());
  _response_body.append("{host_id=\"");
  absl::StrAppend(&_response_body, met.get_host_id());
  _response_body.append("\",service_id=\"");
  absl::StrAppend(&_response_body, met.get_service_id());
  _response_body.append("\"} ");
  boost::apply_visitor(value_to_string_visitor(&_response_body),
                       met.get_value());
  _response_body.push_back(' ');
  absl::StrAppend(&_response_body,
                  std::chrono::duration_cast<std::chrono::milliseconds>(
                      met.get_time().time_since_epoch())
                      .count());
  _response_body.push_back('\n');
}

void http_socket::create_and_start(const beast_stream_ptr& socket,
                                   const endpoint_ptr& endpoint,
                                   const http_server::pointer parent) {
  pointer new_conn(new http_socket(socket, endpoint, parent));
  new_conn->start_read();
}

http_socket::http_socket(const beast_stream_ptr& socket,
                         const endpoint_ptr& endpoint,
                         const http_server::pointer parent)
    : _sock(socket), _endpoint(endpoint), _parent(parent) {
  SPDLOG_LOGGER_DEBUG(parent->get_logger(), "new session {}", *this);
}

http_socket::~http_socket() {
  SPDLOG_LOGGER_DEBUG(_parent->get_logger(), "end session {}", *this);
}

void http_socket::start_read() {
  request_ptr req = std::make_shared<request_type>();
  boost::beast::http::async_read(
      *_sock, _recv_buffer, *req,
      [me = shared_from_this(), req](const boost::beast::error_code& ec,
                                     std::size_t) { me->on_read(ec, req); });
}

void http_socket::on_read(const boost::beast::error_code& ec,
                          const request_ptr& request) {
  if (ec == boost::beast::http::error::end_of_stream) {
    SPDLOG_LOGGER_DEBUG(_parent->get_logger(), "{} socket end: {}", *this,
                        ec.message());
    close();
    return;
  }

  if (ec) {
    SPDLOG_LOGGER_ERROR(_parent->get_logger(), "{} socket read error: {}",
                        *this, ec.message());
    return;
  }
  answer(request);
}

constexpr const char nb_metric_header[] = "nb_metric";

http_socket::response_ptr http_socket::create_response(
    const request_ptr& request) {
  response_ptr resp = std::make_shared<response_type>();
  resp->set(boost::beast::http::field::server, BOOST_BEAST_VERSION_STRING);
  resp->set(boost::beast::http::field::content_type, "text/html");
  resp->keep_alive(request->keep_alive());
  answer_builder payload(resp->body());
  unsigned nb_metric = _parent->pop(_parent->get_bulk_size(), payload);
  resp->prepare_payload();

  SPDLOG_LOGGER_DEBUG(_parent->get_logger(), "prom {} send {} metrics", *this,
                      nb_metric);

  return resp;
}

void http_socket::answer(const request_ptr& request) {
  response_ptr response = create_response(request);
  boost::beast::http::async_write(
      *_sock, *response,
      [me = shared_from_this(), response, request](
          const boost::beast::error_code& err, size_t bytes_transfered) {
        me->on_write(err, bytes_transfered, request, response);
      });
}

void http_socket::on_write(const boost::beast::error_code& err,
                           size_t bytes_transfered,
                           const request_ptr& request,
                           const response_ptr& response) {
  if (err) {
    SPDLOG_LOGGER_ERROR(_parent->get_logger(),
                        "prom {} fail to send metrics: {}", *this,
                        err.message());
    return;
  }
  SPDLOG_LOGGER_DEBUG(_parent->get_logger(), "prom {} metrics sent", *this);
  start_read();
}

void http_socket::close() {
  boost::beast::error_code err;
  _sock->socket().shutdown(boost::asio::ip::tcp::socket::shutdown_both, err);
}

}  // namespace detail
}  // namespace prometheus

/******************************************************************************************
 *
 *          listener http_server
 *
 ******************************************************************************************/

http_server::http_server(const io_context_ptr& io_context,
                         const boost::json::object& conf,
                         const logger_ptr& logger,
                         unsigned bulk_size)
    : _io_context(io_context),
      _logger(logger),
      _bulk_size(bulk_size),
      _acceptor(*io_context),
      _listen_endpoint(boost::asio::ip::address_v4::any(),
                       conf.at("port").as_int64()) {}

void http_server::start() {
  _acceptor.open(_listen_endpoint.protocol());
  _acceptor.set_option(boost::asio::socket_base::reuse_address(true));
  _acceptor.bind(_listen_endpoint);
  _acceptor.listen();
  start_accept();
}

void http_server::start_accept() {
  endpoint_ptr client(std::make_shared<boost::asio::ip::tcp::endpoint>());
  beast_stream_ptr sock(
      std::make_shared<boost::beast::tcp_stream>(*_io_context));
  _acceptor.async_accept(
      sock->socket(), *client,
      [me = shared_from_this(), client, sock](const std::error_code& err) {
        me->accept(err, sock, client);
      });
}

void http_server::accept(const std::error_code& err,
                         const beast_stream_ptr& sock,
                         const endpoint_ptr& endpt) {
  if (err) {
    SPDLOG_LOGGER_ERROR(_logger, "http_server::accept fail to accept {}",
                        err.message());
    return;
  }
  detail::http_socket::create_and_start(sock, endpt, shared_from_this());
  start_accept();
}