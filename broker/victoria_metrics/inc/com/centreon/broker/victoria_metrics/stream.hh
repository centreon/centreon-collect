/*
** Copyright 2022 Centreon
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

#ifndef CCB_VICTORIA_METRICS_STREAM_HH
#define CCB_VICTORIA_METRICS_STREAM_HH

#include "victoria_config.hh"

#include "com/centreon/broker/http_tsdb/line_protocol_query.hh"
#include "com/centreon/broker/http_tsdb/stream.hh"

CCB_BEGIN()

// Forward declaration.
class database_config;

namespace victoria_metrics {

class stream : public http_tsdb::stream {
  unsigned _body_size_to_reserve;

  http_tsdb::line_protocol_query _metric_formatter;
  http_tsdb::line_protocol_query _status_formatter;

  std::string _hostname;

 protected:
  stream(const std::shared_ptr<asio::io_context>& io_context,
         const std::shared_ptr<victoria_config>& conf,
         const std::shared_ptr<persistent_cache>& cache,
         http_client::client::connection_creator conn_creator =
             http_client::http_connection::load);

  http_tsdb::request::pointer create_request() const override;

 public:
  static std::shared_ptr<stream> load(
      const std::shared_ptr<asio::io_context>& io_context,
      const std::shared_ptr<victoria_config>& conf,
      const std::shared_ptr<persistent_cache>& cache,
      http_client::client::connection_creator conn_creator =
          http_client::http_connection::load);
};

/**
 * @brief stream is asynchronous and so needs to inherit from
 * enable_shared_from_this
 * connector needs a unique_ptr, this little class does the conversion
 *
 */
class stream_unique_wrapper : public io::stream {
  std::shared_ptr<stream> _stream;

 public:
  stream_unique_wrapper(const std::shared_ptr<stream>& stream)
      : io::stream(stream->get_name()), _stream(stream) {}

  int32_t stop() override { return _stream->stop(); }
  int flush() override { return _stream->flush(); }
  bool read(std::shared_ptr<io::data>& d, time_t deadline) override {
    return _stream->read(d, deadline);
  }
  void statistics(nlohmann::json& tree) const override {
    return _stream->statistics(tree);
  }
  int write(std::shared_ptr<io::data> const& d) override {
    return _stream->write(d);
  }
};
};  // namespace victoria_metrics

CCB_END()

#endif  // !CCB_VICTORIA_METRICS_STREAM_HH
