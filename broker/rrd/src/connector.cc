/**
 * Copyright 2011-2014 Centreon
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

#include "com/centreon/broker/rrd/connector.hh"

#include "bbdo/storage/metric.hh"
#include "bbdo/storage/remove_graph.hh"
#include "bbdo/storage/status.hh"
#include "com/centreon/broker/rrd/internal.hh"
#include "com/centreon/broker/rrd/output.hh"
#include "common/log_v2/log_v2.hh"

using namespace com::centreon::broker;
using namespace com::centreon::broker::rrd;
using log_v2 = com::centreon::common::log_v2::log_v2;

static constexpr multiplexing::muxer_filter _rrd_stream_filter =
    multiplexing::muxer_filter(
        {storage::metric::static_type(), storage::pb_metric::static_type(),
         storage::status::static_type(), storage::pb_status::static_type(),
         storage::pb_rebuild_message::static_type(),
         storage::remove_graph::static_type(),
         storage::pb_remove_graph_message::static_type(),
         make_type(io::extcmd, extcmd::de_pb_bench)});

static constexpr multiplexing::muxer_filter _rrd_forbidden_filter =
    multiplexing::muxer_filter(_rrd_stream_filter).reverse();

/**
 *  Default constructor.
 */
connector::connector()
    : io::endpoint(false, _rrd_stream_filter, _rrd_forbidden_filter),
      _cache_size(16),
      _cached_port(0),
      _ignore_update_errors(true),
      _write_metrics(true),
      _write_status(true) {}

/**
 *  Connect.
 *
 *  @return Stream object.
 */
std::shared_ptr<io::stream> connector::open() {
  std::shared_ptr<io::stream> retval;
  if (!_cached_local.empty())
    retval.reset(new output<cached<asio::local::stream_protocol::socket>>(
        _metrics_path, _status_path, _cache_size, _ignore_update_errors,
        _cached_local, _write_metrics, _write_status));
  else if (_cached_port)
    retval.reset(new output<cached<asio::ip::tcp::socket>>(
        _metrics_path, _status_path, _cache_size, _ignore_update_errors,
        _cached_port, _write_metrics, _write_status));
  else
    retval.reset(new output<lib>(_metrics_path, _status_path, _cache_size,
                                 _ignore_update_errors, _write_metrics,
                                 _write_status));
  return retval;
}

/**
 *  Set the rrd creator cache size.
 *
 *  @param[in] cache_size The rrd creator cache size.
 */
void connector::set_cache_size(uint32_t cache_size) {
  _cache_size = cache_size;
}

/**
 *  Set the local socket path.
 *
 *  @param[in] local_socket Local socket path.
 */
void connector::set_cached_local(std::string const& local_socket) {
  _cached_local = local_socket;
}

/**
 *  Set the network connection port.
 *
 *  @param[in] port rrdcached port.
 */
void connector::set_cached_net(uint16_t port) noexcept {
  _cached_port = port;
}

/**
 *  Set if update errors must be checked or not (2.4.0-compatible
 *  behavior).
 *
 *  @param[in] ignore Set to true to ignore update errors.
 */
void connector::set_ignore_update_errors(bool ignore) noexcept {
  _ignore_update_errors = ignore;
}

/**
 *  Set the RRD metrics path.
 *
 *  @param[in] metrics_path Where metrics RRD files will be written.
 */
void connector::set_metrics_path(std::string const& metrics_path) {
  _metrics_path = _real_path_of(metrics_path);
}

/**
 *  Set the RRD status path.
 *
 *  @param[in] status_path Where status RRD files will be written.
 */
void connector::set_status_path(std::string const& status_path) {
  _status_path = _real_path_of(status_path);
}

/**
 *  Set whether or not metrics should be written.
 *
 *  @param[in] write_metrics true if metrics must be written.
 */
void connector::set_write_metrics(bool write_metrics) noexcept {
  _write_metrics = write_metrics;
}

/**
 *  Set whether or not status should be written.
 *
 *  @param[in] write_status true if status must be written.
 */
void connector::set_write_status(bool write_status) noexcept {
  _write_status = write_status;
}

/**************************************
 *                                     *
 *           Private Methods           *
 *                                     *
 **************************************/

/**
 *  Get the real path (absolute, expanded) of a path.
 *
 *  @param[in] path Path to resolve.
 *
 *  @return Real path.
 */
std::string connector::_real_path_of(std::string const& path) {
  // Variables.
  std::string retval;
  char* real_path{realpath(path.c_str(), nullptr)};
  auto logger = log_v2::instance().get(log_v2::RRD);

  // Resolution success.
  if (real_path) {
    logger->info("RRD: path '{}' resolved as '{}'", path, real_path);
    try {
      retval = real_path;
    } catch (...) {
      free(real_path);
      throw;
    }
    free(real_path);
  }
  // Resolution failure.
  else {
    char const* msg{strerror(errno)};
    logger->error("RRD: could not resolve path '{}', using it as such: {}",
                  path, msg);
    retval = path;
  }

  // Last slash.
  int last_index{static_cast<int>(retval.size()) - 1};
  if (!retval.empty() && retval[last_index] != '/')
    retval.append("/");

  return retval;
}
