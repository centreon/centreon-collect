/**
 * Copyright 2022-2024 Centreon
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

#ifndef CCB_VICTORIA_METRICS_STREAM_HH
#define CCB_VICTORIA_METRICS_STREAM_HH

#include "com/centreon/broker/http_tsdb/stream.hh"
#include "com/centreon/broker/victoria_metrics/line_protocol_query.hh"
#include "com/centreon/broker/victoria_metrics/series_config.hh"
#include "bbdo/rebuild_message.pb.h"
#include "bbdo/remove_graph_message.pb.h"

#include <absl/container/flat_hash_map.h>

namespace com::centreon::broker {

namespace storage {
// Messages of unified_sql to the graph outputs, as declared by the RRD output.
using pb_rebuild_message =
    io::protobuf<RebuildMessage,
                 make_type(io::storage, storage::de_rebuild_message)>;
using pb_remove_graph_message =
    io::protobuf<RemoveGraphMessage,
                 make_type(io::storage, storage::de_remove_graph_message)>;
}  // namespace storage

// Forward declaration.
class database_config;

namespace victoria_metrics {

class stream : public http_tsdb::stream {
  unsigned _body_size_to_reserve;

  line_protocol_query _metric_formatter;
  line_protocol_query _status_formatter;

  std::string _authorization;
  std::string _account_id;
  std::string _write_target;
  series_config _series_conf;

  // metric_id -> index_id of the metrics being rebuilt
  absl::flat_hash_map<uint64_t, uint64_t> _rebuilt;

  int _ack_one();
  void _remember_labels(const std::shared_ptr<io::data>& d);
  void _rebuild_start(const RebuildMessage& msg);
  void _rebuild_data(const RebuildMessage& msg);
  void _rebuild_end(const RebuildMessage& msg);
  void _resolve_labels(const std::vector<uint64_t>& metric_ids);
  void _delete_series(const std::vector<std::string>& selectors);
  std::shared_ptr<http::request_base> _create_form_request(
      const std::string& target,
      const std::vector<std::string>& selectors,
      size_t from,
      size_t to) const;

 protected:
  stream(const std::shared_ptr<asio::io_context>& io_context,
         const std::shared_ptr<http_tsdb::http_tsdb_config>& conf,
         const std::string& account_id,
         http::connection_creator conn_creator,
         const series_config& series_conf);

  http_tsdb::request::pointer create_request() const override;

 public:
  static const std::string allowed_macros;

  static std::shared_ptr<stream> load(
      const std::shared_ptr<asio::io_context>& io_context,
      const std::shared_ptr<http_tsdb::http_tsdb_config>& conf,
      const std::string& account_id,
      http::connection_creator conn_creator,
      const series_config& series_conf = series_config());

  /**
   * @brief Write metrics and status as the base class does, and handle the
   * rebuild and remove graph messages of unified_sql.
   */
  int write(const std::shared_ptr<io::data>& d) override;

  const std::string& get_authorization() const { return _authorization; }
  const std::string& get_write_target() const { return _write_target; }

  /**
   * @brief Add precision=s to a write target that does not set a precision:
   * Broker writes timestamps in seconds.
   */
  static std::string write_target_with_precision(const std::string& target);

  /**
   * @brief Encode a value for an application/x-www-form-urlencoded body.
   */
  static std::string form_encode(const std::string& value);
};

};  // namespace victoria_metrics

}  // namespace com::centreon::broker

#endif  // !CCB_VICTORIA_METRICS_STREAM_HH
