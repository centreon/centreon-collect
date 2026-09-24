/**
 * Copyright 2022 Centreon
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

#ifndef CCB_VICTORIA_LINE_PROTOCOL_QUERY_HH
#define CCB_VICTORIA_LINE_PROTOCOL_QUERY_HH

#include "com/centreon/broker/http_tsdb/column.hh"
#include "com/centreon/broker/http_tsdb/internal.hh"
#include "com/centreon/broker/http_tsdb/line_protocol_query.hh"
#include "neb.pb.h"

namespace com::centreon::broker::victoria_metrics {

/**
 *  @class line_protocol_query line_protocol_query.hh
 * "com/centreon/broker/graphite/line_protocol_query.hh"
 *  @brief Query compiling/generation.
 *
 *  This class compiles a query for further uses, generating
 *  the query fast.
 */
class line_protocol_query : public http_tsdb::line_protocol_query {
 public:
  line_protocol_query(const std::string allowed_macros,
                      std::vector<http_tsdb::column> const& columns,
                      data_type type,
                      const std::shared_ptr<spdlog::logger>& logger);
  line_protocol_query(line_protocol_query const& other) = delete;
  ~line_protocol_query() = default;
  static void escape_key(std::string const& str, std::ostream& is);
  static void escape_value(std::string const& str, std::ostream& is);
};

}  // namespace com::centreon::broker::victoria_metrics

#endif  // !CCB_HTTP_TSDB_LINE_PROTOCOL_QUERY_HH
