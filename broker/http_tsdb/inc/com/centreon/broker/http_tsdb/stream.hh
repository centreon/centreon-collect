/*
** Copyright 2011-2017 Centreon
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
**
** For more information : contact@centreon.com
*/

#ifndef CCB_HTTP_TSDB_STREAM_HH
#define CCB_HTTP_TSDB_STREAM_HH

#include "com/centreon/broker/http_client/http_client.hh"
#include "com/centreon/broker/io/stream.hh"
#include "com/centreon/broker/namespace.hh"
#include "com/centreon/broker/persistent_cache.hh"
#include "http_tsdb_config.hh"
#include "line_protocol_query.hh"
#include "macro_cache.hh"

namespace http_client = com::centreon::broker::http_client;

CCB_BEGIN()

namespace http_tsdb {

class request : public http_client::request_type {
 protected:
  unsigned _nb_metric;
  unsigned _nb_status;

 public:
  using pointer = std::shared_ptr<request>;

  request() {}
  request(boost::beast::http::verb method,
          boost::beast::string_view target,
          unsigned version = 11)
      : http_client::request_type(method, target, version),
        _nb_metric(0),
        _nb_status(0) {}

  virtual void add_metric(const storage::metric& metric) = 0;
  virtual void add_metric(const Metric& metric) = 0;

  virtual void add_status(const storage::status& status) = 0;
  virtual void add_status(const Status& status) = 0;

  virtual void append(const request::pointer& data_to_append);

  unsigned get_nb_metric() const { return _nb_metric; }
  unsigned get_nb_status() const { return _nb_status; }
  unsigned get_nb_data() const { return _nb_metric + _nb_status; }
};

/**
 *  @class stream stream.hh "com/centreon/broker/influxdb/stream.hh"
 *  @brief tsdb stream.
 *
 *  Insert metrics into tsdb.
 */
class stream : public io::stream, public std::enable_shared_from_this<stream> {
 protected:
  std::shared_ptr<asio::io_context> _io_context;
  const std::shared_ptr<spdlog::logger> _logger;
  // Database and http parameters
  std::shared_ptr<http_tsdb_config> _conf;

  // Cache
  macro_cache _cache;

  http_client::client::pointer _http_client;

  unsigned _acknowledged;
  request::pointer _request;
  // this timer is used to send periodicaly datas even if we haven't yet
  // _conf->_max_queries_per_transaction events to send
  asio::system_timer _timeout_send_timer;
  std::atomic_bool _timeout_send_timer_run;
  mutable std::mutex _protect;

  stream(const std::string& name,
         const std::shared_ptr<asio::io_context>& io_context,
         const std::shared_ptr<spdlog::logger>& logger,
         const std::shared_ptr<http_tsdb_config>& conf,
         const std::shared_ptr<persistent_cache>& cache,
         http_client::client::connection_creator conn_creator =
             http_client::http_connection::load);

  virtual request::pointer create_request() const = 0;

  void send_request(const request::pointer& request);

  // used by flush only, avoid it
  void send_request(const request::pointer& request,
                    const std::shared_ptr<std::promise<void>>& prom);

  void send_handler(const boost::beast::error_code& err,
                    const std::string& detail,
                    const request::pointer& request,
                    const http_client::response_ptr& response);

  void start_timeout_send_timer();
  void timeout_send_timer_handler(const boost::system::error_code& err);

 public:
  using pointer = std::shared_ptr<stream>;

  ~stream();
  int flush() override;
  bool read(std::shared_ptr<io::data>& d, time_t deadline) override;
  void statistics(nlohmann::json& tree) const override;
  int write(std::shared_ptr<io::data> const& d) override;
  int32_t stop() override;
};
}  // namespace http_tsdb

CCB_END()

#endif  // !CCB_HTTP_TSDB_STREAM_HH
