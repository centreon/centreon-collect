/**
 * Copyright 2011-2015,2017 Centreon
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

#include "com/centreon/broker/graphite/stream.hh"
#include "bbdo/storage/metric.hh"
#include "com/centreon/broker/exceptions/shutdown.hh"
#include "com/centreon/broker/io/events.hh"
#include "com/centreon/broker/misc/string.hh"
#include "com/centreon/broker/multiplexing/engine.hh"
#include "com/centreon/broker/multiplexing/publisher.hh"
#include "com/centreon/exceptions/msg_fmt.hh"
#include "common/crypto/base64.hh"
#include "common/log_v2/log_v2.hh"

using namespace asio;
using namespace com::centreon::exceptions;
using namespace com::centreon::broker;
using namespace com::centreon::broker::graphite;
using log_v2 = com::centreon::common::log_v2::log_v2;

/**
 *  Constructor.
 *
 */
stream::stream(std::string const& metric_naming,
               std::string const& status_naming,
               std::string const& escape_string,
               std::string const& db_user,
               std::string const& db_password,
               std::string const& db_host,
               unsigned short db_port,
               uint32_t queries_per_transaction,
               std::shared_ptr<persistent_cache> const& cache)
    : io::stream("graphite"),
      _metric_naming{metric_naming},
      _status_naming{status_naming},
      _db_user{db_user},
      _db_password{db_password},
      _db_host{db_host},
      _db_port{db_port},
      _queries_per_transaction{
          (queries_per_transaction == 0) ? 1 : queries_per_transaction},
      _pending_queries{0},
      _actual_query{0},
      _commit_flag{false},
      _metric_query{_metric_naming, escape_string, query::metric, _cache},
      _status_query{_status_naming, escape_string, query::status, _cache},
      _socket{_io_context},
      _logger{log_v2::instance().get(log_v2::GRAPHITE)},
      _cache{cache} {
  _logger->trace("graphite::stream constructor {}", static_cast<void*>(this));
  // Create the basic HTTP authentification header.
  if (!_db_user.empty() && !_db_password.empty()) {
    std::string auth = fmt::format("{}:{}", _db_user, _db_password);
    _auth_query = fmt::format("Authorization: Basic {}\n",
                              common::crypto::base64_encode(auth));
    _query.append(_auth_query);
  }

  boost::system::error_code err;
  ip::tcp::resolver resolver{_io_context};

  auto endpoint = resolver.resolve(_db_host, std::to_string(_db_port), err);

  if (err) {
    throw msg_fmt(
        "graphite: can't resolve graphite on host '{}', port '{}' : {}",
        _db_host, _db_port, err.message());
  }

  asio::connect(_socket, endpoint, err);
  if (err) {
    throw msg_fmt(
        "graphite: can't connect to graphite on host '{}', port '{}' : {}",
        _db_host, _db_port, err.message());
  }
}

/**
 *  Destructor.
 */
stream::~stream() {
  _logger->trace("graphite::stream destructor {}", static_cast<void*>(this));
}

/**
 *  Flush the stream.
 *
 *  @return Number of events acknowledged.
 */
int32_t stream::flush() {
  _logger->debug("graphite: commiting {} queries", _actual_query);
  int32_t ret(_pending_queries);
  if (_actual_query != 0)
    _commit();
  _actual_query = 0;
  _pending_queries = 0;
  _commit_flag = false;
  return ret;
}

/**
 * @brief Flush the stream and stop it.
 *
 * @return the number of acknowledged events.
 */
int32_t stream::stop() {
  _logger->trace("graphite::stream stop {}", static_cast<void*>(this));
  int32_t retval = flush();
  _logger->info("graphite stopped with {} events acknowledged", retval);
  return retval;
}

/**
 *  Read from the database.
 *
 *  @param[out] d         Cleared.
 *  @param[in]  deadline  Timeout.
 *
 *  @return This method will throw.
 */
bool stream::read(std::shared_ptr<io::data>& d, time_t deadline) {
  (void)deadline;
  d.reset();
  throw exceptions::shutdown("cannot read from Graphite database");
  return true;
}

/**
 *  Get endpoint statistics.
 *
 *  @param[out] tree Output tree.
 */
void stream::statistics(nlohmann::json& tree) const {
  std::lock_guard<std::mutex> lock(_statusm);
  if (!_status.empty())
    tree["status"] = _status;
}

/**
 *  Write an event.
 *
 *  @param[in] data Event pointer.
 *
 *  @return Number of events acknowledged.
 */
int stream::write(std::shared_ptr<io::data> const& data) {
  // Take this event into account.
  ++_pending_queries;
  if (!validate(data, get_name()))
    return 0;

  // Give the event to the cache.
  _cache.write(data);

  // Process metric events.
  switch (data->type()) {
    case storage::metric::static_type():
      if (_process_metric(
              *std::static_pointer_cast<storage::metric const>(data)))
        ++_actual_query;
      break;
    case storage::pb_metric::static_type():
      if (_process_metric(
              *std::static_pointer_cast<storage::pb_metric const>(data)))
        ++_actual_query;
      break;
    case storage::status::static_type():
      if (_process_status(
              *std::static_pointer_cast<storage::status const>(data)))
        ++_actual_query;
      break;
    case storage::pb_status::static_type():
      if (_process_status(
              *std::static_pointer_cast<storage::pb_status const>(data)))
        ++_actual_query;
      break;
  }
  if (_actual_query >= _queries_per_transaction)
    _commit_flag = true;

  if (_commit_flag)
    return flush();
  else
    return 0;
}

/**
 *  Process a metric event.
 *
 *  @param[in] me  The event to process.
 */
bool stream::_process_metric(storage::metric const& me) {
  storage::pb_metric converted;
  me.convert_to_pb(converted.mut_obj());
  return _process_metric(converted);
}

/**
 *  Process a metric event.
 *
 *  @param[in] me  The event to process.
 */
bool stream::_process_metric(storage::pb_metric const& me) {
  std::string to_append = _metric_query.generate_metric(me);
  _query.append(to_append);
  return !to_append.empty();
}

/**
 *  Process a status event.
 *
 *  @param[in] st  The status event.
 */
bool stream::_process_status(storage::status const& st) {
  storage::pb_status converted;
  st.convert_to_pb(converted.mut_obj());
  return _process_status(converted);
}

/**
 *  Process a status event.
 *
 *  @param[in] st  The status event.
 */
bool stream::_process_status(storage::pb_status const& st) {
  std::string to_append = _status_query.generate_status(st);
  _query.append(to_append);
  return !to_append.empty();
}

/**
 *  Commit all the processed event to the database.
 */
void stream::_commit() {
  if (!_query.empty()) {
    boost::system::error_code err;

    asio::write(_socket, buffer(_query), asio::transfer_all(), err);
    if (err)
      throw msg_fmt(
          "graphite: can't send data to graphite on host '{}', port '{}' : {}",
          _db_host, _db_port, err.message());

    _query.clear();
    _query.append(_auth_query);
  }
}
