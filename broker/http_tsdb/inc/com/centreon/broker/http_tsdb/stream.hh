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

#include "com/centreon/broker/persistent_cache.hh"
#include "http_tsdb_config.hh"
#include "internal.hh"
#include "line_protocol_query.hh"

namespace http_client = com::centreon::broker::http_client;

namespace com::centreon::broker {

namespace http_tsdb {

class request : public http_client::request_base {
 protected:
  unsigned _nb_metric;
  unsigned _nb_status;

 public:
  using pointer = std::shared_ptr<request>;

  request() : _nb_metric(0), _nb_status(0) {}
  request(boost::beast::http::verb method,
          const std::string& server_name,
          boost::beast::string_view target,
          unsigned version = 11)
      : http_client::request_base(method, server_name, target),
        _nb_metric(0),
        _nb_status(0) {}

  void dump(std::ostream&) const override;

  virtual void add_metric(const storage::pb_metric& metric) = 0;

  virtual void add_status(const storage::pb_status& status) = 0;

  virtual void append(const request::pointer& data_to_append);

  unsigned get_nb_metric() const { return _nb_metric; }
  unsigned get_nb_status() const { return _nb_status; }
  unsigned get_nb_data() const { return _nb_metric + _nb_status; }
};

inline std::ostream& operator<<(std::ostream& str, const request& req) {
  (&req)->dump(str);
  return str;
}

/**
 *  @class stream stream.hh "com/centreon/broker/influxdb/stream.hh"
 *  @brief tsdb stream.
 *  This class is a base class
 *  it doesn't care about the request format, it's the job of the final class
 *
 *  Insert metrics into tsdb.
 */
class stream : public io::stream, public std::enable_shared_from_this<stream> {
 protected:
  std::shared_ptr<asio::io_context> _io_context;
  // Database and http parameters
  std::shared_ptr<http_tsdb_config> _conf;

  http_client::client::pointer _http_client;

  // number of metric and status sent to tsdb and acknowledged by a 20x response
  unsigned _acknowledged;
  // the current request that buffers metric to send
  request::pointer _request;
  // the two beans stat_unit and stat_average are used to produce statistics
  // about request time
  /**
   * @brief stat cumul
   * this bean is used to cumulate request for example during one second
   */
  struct stat_unit {
    time_t time;
    unsigned value;
  };

  using stat = stat_unit[2];

  stat _success_request_stat;
  stat _failed_request_stat;
  stat _metric_stat;
  stat _status_stat;

  /* Logger */
  std::shared_ptr<spdlog::logger> _logger;

  /**
   * @brief this cless calc an average over a period
   *
   */
  class stat_average {
    std::map<time_point, unsigned> _points;

   public:
    void add_point(unsigned value);
    bool empty() const { return _points.empty(); }
    unsigned get_average() const;
  };

  stat_average _connect_avg;
  stat_average _send_avg;
  stat_average _recv_avg;

  mutable std::mutex _protect;

  stream(const std::string& name,
         const std::shared_ptr<asio::io_context>& io_context,
         const std::shared_ptr<http_tsdb_config>& conf,
         http_client::client::connection_creator conn_creator =
             http_client::http_connection::load);

  virtual request::pointer create_request() const = 0;

  void send_request(const request::pointer& request);

  void send_handler(const boost::beast::error_code& err,
                    const std::string& detail,
                    const request::pointer& request,
                    const http_client::response_ptr& response);

  void add_to_stat(stat& to_maj, unsigned to_add);

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

}  // namespace com::centreon::broker

#endif  // !CCB_HTTP_TSDB_STREAM_HH
