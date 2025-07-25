/**
 * Copyright 2011-2017, 2024 Centreon
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * For more information : contact@centreon.com
 */

#include "com/centreon/broker/influxdb/influxdb.hh"
#include <iterator>
#include "com/centreon/broker/misc/string.hh"
#include "com/centreon/exceptions/msg_fmt.hh"
#include "common/log_v2/log_v2.hh"

using namespace asio;
using namespace com::centreon::exceptions;
using namespace com::centreon::broker::influxdb;
using log_v2 = com::centreon::common::log_v2::log_v2;

static const char* query_footer = "\n";

/**
 *  Constructor.
 */
influxdb::influxdb(std::string const& user,
                   std::string const& passwd,
                   std::string const& addr,
                   unsigned short port,
                   std::string const& db,
                   std::string const& status_ts,
                   std::vector<column> const& status_cols,
                   std::string const& metric_ts,
                   std::vector<column> const& metric_cols,
                   macro_cache const& cache,
                   const std::shared_ptr<spdlog::logger>& logger)
    : _socket{_io_context},
      _host(addr),
      _port(port),
      _cache(cache),
      _logger{logger} {
  // Try to connect to the server.
  _logger->debug("influxdb: connecting using 1.2 Line Protocol");
  _connect_socket();
  _create_queries(user, passwd, db, status_ts, status_cols, metric_ts,
                  metric_cols);
}

/**
 *  Clear the query.
 */
void influxdb::clear() {
  _query.clear();
}

/**
 *  Write a metric to the query.
 *
 *  @param[in] m  The metric to write.
 */
void influxdb::write(storage::metric const& m) {
  storage::pb_metric converted;
  m.convert_to_pb(converted.mut_obj());
  _query.append(_metric_query.generate_metric(converted));
}

/**
 *  Write a status to the query.
 *
 *  @param[in] s  The status to write.
 */
void influxdb::write(storage::status const& s) {
  storage::pb_status converted;
  s.convert_to_pb(converted.mut_obj());
  _query.append(_status_query.generate_status(converted));
}

/**
 *  Write a metric to the query.
 *
 *  @param[in] m  The metric to write.
 */
void influxdb::write(const storage::pb_metric& m) {
  _query.append(_metric_query.generate_metric(m));
}

/**
 *  Write a status to the query.
 *
 *  @param[in] s  The status to write.
 */
void influxdb::write(const storage::pb_status& s) {
  _query.append(_status_query.generate_status(s));
}

/**
 *  Commit a query.
 */
void influxdb::commit() {
  if (_query.empty())
    return;

  std::stringstream content_length;
  size_t length = _query.size() + ::strlen(query_footer);
  content_length << "Content-Length: " << length << "\n";

  std::string final_query;
  final_query.reserve(length + _post_header.size() +
                      content_length.str().size() + 1);
  final_query.append(_post_header)
      .append(content_length.str())
      .append("\n")
      .append(_query)
      .append(query_footer);

  _connect_socket();
  boost::system::error_code ec;
  auto re{_socket.remote_endpoint(ec)};
  if (ec)
    throw msg_fmt("influxdb: unable to get remote endpoint from socket: {}",
                  ec.message());
  std::string addr{re.address().to_string()};
  uint16_t port = re.port();

  boost::system::error_code err;

  asio::write(_socket, buffer(final_query), asio::transfer_all(), err);
  if (err)
    throw msg_fmt(
        "influxdb: couldn't commit data to InfluxDB with address '{}"
        "' and port '{}': {}",
        addr, port, err.message());
  // Receive the server answer.

  std::string answer;
  std::size_t total_read{0}, read_size{2048};

  do {
    answer.resize(read_size);

    total_read += _socket.read_some(
        asio::buffer(&answer[total_read], read_size - total_read), err);
    if (total_read == read_size)
      total_read += 2048;

    answer.resize(total_read);

    if (err)
      throw msg_fmt(
          "influxdb: couldn't receive InfluxDB answer with address '{}"
          "' and port '{}': {}",
          addr, port, err.message());

  } while (!_check_answer_string(answer, addr, port));
  _socket.shutdown(ip::tcp::socket::shutdown_both);
  _socket.close();
  _query.clear();
}

/**
 *  Connect the socket to the endpoint.
 */
void influxdb::_connect_socket() {
  if (_socket.is_open()) {
    _socket.shutdown(ip::tcp::socket::shutdown_both);
    _socket.close();
  }
  boost::system::error_code err;
  ip::tcp::resolver resolver{_io_context};
  auto endpoints = resolver.resolve(_host, std::to_string(_port), err);
  if (err) {
    throw msg_fmt(
        "influxdb: couldn't resolve InfluxDB with address '{}"
        "' and port '{}': {}",
        _host, _port, err.message());
  }
  // Try to connect to the server.
  asio::connect(_socket, endpoints, err);
  if (err) {
    throw msg_fmt(
        "influxdb: couldn't connect to InfluxDB with address '{}"
        "' and port '{}': {}",
        _host, _port, err.message());
  }
}

/**
 *  Check the server's answer.
 *
 *  @param[in] ans  The server's answer.
 *  @param[in] addr The server's address as std::string.
 *  @param[in] port  The server's port.
 *
 *  @return         True of the answer was complete, false otherwise.
 */
bool influxdb::_check_answer_string(std::string const& ans,
                                    const std::string& addr,
                                    uint16_t port) {
  size_t first_line = ans.find_first_of('\n');
  if (first_line == std::string::npos)
    return false;
  std::string first_line_str = ans.substr(0, first_line);

  _logger->debug("influxdb: received an anwser from {}:{}: {}", addr, port,
                 ans);

  // Split the first line using the power of std.
  std::istringstream iss(first_line_str);
  std::vector<std::string> split;
  std::copy(std::istream_iterator<std::string>(iss),
            std::istream_iterator<std::string>(), std::back_inserter(split));

  if (split.size() < 3)
    throw msg_fmt(
        "influxdb: unrecognizable HTTP header for '{}' and port '{}'"
        ": got '{}'",
        addr, port, first_line_str);

  if (split[0] == "HTTP/1.0" && split[1] == "204" && split[2] == "No" &&
      split[3] == "Content")
    return true;
  else if (ans.find("partial write: points beyond retention policy dropped") !=
           std::string::npos) {
    _logger->info(
        "influxdb: sending points beyond "
        "Influxdb database configured "
        "retention policy");
    return true;
  } else
    throw msg_fmt("influxdb: got an error from '{}' and port '{}': '{}'", addr,
                  port, ans);
}

/**
 *  Create the queries for influxdb.
 *
 *  @param[in] status_ts    Name of the timeseries status.
 *  @param[in] status_cols  Column for the statuses.
 *  @param[in] metric_ts    Name of the timeseries metric.
 *  @param[in] metric_cols  Column for the metrics.
 */
void influxdb::_create_queries(std::string const& user,
                               std::string const& passwd,
                               std::string const& db,
                               std::string const& status_ts,
                               std::vector<column> const& status_cols,
                               std::string const& metric_ts,
                               std::vector<column> const& metric_cols) {
  // Create POST HTTP header.
  std::string base_url;
  base_url.append("/write?u=")
      .append(user)
      .append("&p=")
      .append(passwd)
      .append("&db=")
      .append(db)
      .append("&precision=s");
  _post_header.append("POST ").append(base_url).append(" HTTP/1.0\n");

  // Create protocol objects.
  _status_query = line_protocol_query(status_ts, status_cols,
                                      line_protocol_query::status, _cache);
  _metric_query = line_protocol_query(metric_ts, metric_cols,
                                      line_protocol_query::metric, _cache);
}
