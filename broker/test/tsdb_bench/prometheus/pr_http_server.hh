#ifndef __TSDB__BENCH__PROMETHEUS_SERVER_HH
#define __TSDB__BENCH__PROMETHEUS_SERVER_HH

#include "metric.hh"

namespace prometheus {
using endpoint_ptr = std::shared_ptr<boost::asio::ip::tcp::endpoint>;
using beast_stream_ptr = std::shared_ptr<boost::beast::tcp_stream>;

class http_server : public std::enable_shared_from_this<http_server> {
  io_context_ptr _io_context;
  logger_ptr _logger;
  unsigned _bulk_size;
  boost::asio::ip::tcp::acceptor _acceptor;
  boost::asio::ip::tcp::endpoint _listen_endpoint;

  using queue = std::queue<metric>;
  queue _queue;

  std::mutex _protect;

  void start_accept();

  void accept(const std::error_code& err,
              const beast_stream_ptr& sock,
              const endpoint_ptr& endpt);

 public:
  using pointer = std::shared_ptr<http_server>;

  http_server(const io_context_ptr& io_context,
              const boost::json::object& conf,
              const logger_ptr& logger,
              unsigned bulk_size);

  logger_ptr get_logger() const { return _logger; }
  constexpr const unsigned& get_bulk_size() const { return _bulk_size; }

  void start();

  template <class metric_iterator>
  void push(metric_iterator begin, metric_iterator end);

  template <class inserter>
  unsigned pop(unsigned max_to_pop, inserter& ins);
};

template <class metric_iterator>
void http_server::push(metric_iterator begin, metric_iterator end) {
  std::lock_guard<std::mutex> l(_protect);
  for (; begin != end; ++begin) {
    _queue.push(*begin);
  }
}

template <class inserter>
unsigned http_server::pop(unsigned max_to_pop, inserter& ins) {
  std::lock_guard<std::mutex> l(_protect);
  if (max_to_pop > _queue.size()) {
    max_to_pop = _queue.size();
  }
  for (unsigned pop_counter = 0; pop_counter < max_to_pop; ++pop_counter) {
    ins(_queue.front());
    _queue.pop();
  }

  return max_to_pop;
}

};  // namespace prometheus

#endif
