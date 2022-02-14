#include "com/centreon/broker/grpc/connector.hh"
#include "com/centreon/broker/grpc/stream.hh"

using namespace com::centreon::broker;
using namespace com::centreon::broker::grpc;

/**
 * @brief Constructor of the connector that will connect to the given host at
 * the given port. read_timeout is a duration in seconds or -1 if no limit.
 *
 * @param host The host to connect to.
 * @param port The port used for the connection.
 */
connector::connector(const std::string& host, uint16_t port)
    : io::limit_endpoint(false), _hostport(host + ':' + std::to_string(port)) {}

/**
 * @brief open a new connection
 *
 * @return std::unique_ptr<io::stream>
 */
std::unique_ptr<io::stream> connector::open() {
  log_v2::grpc()->info("TCP: connecting to {}", _hostport);
  try {
    return limit_endpoint::open();
  } catch (const std::exception& e) {
    log_v2::tcp()->debug(
        "Unable to establish the connection to {} (attempt {}): {}", _hostport,
        _is_ready_count, e.what());
    throw;
  }
}

/**
 * @brief create a stream from attributes
 *
 * @return std::unique_ptr<io::stream>
 */
std::unique_ptr<io::stream> connector::create_stream() {
  return std::make_unique<stream>(_hostport);
}
