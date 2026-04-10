/**
 * Copyright 2015,2017, 2023-2024 Centreon
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

#ifndef CCB_GRAPHITE_QUERY_HH
#define CCB_GRAPHITE_QUERY_HH

#include "com/centreon/broker/http_tsdb/line_protocol_query.hh"
#include "internal.hh"
namespace com::centreon::broker::graphite {

/**
 *  @class query query.hh "com/centreon/broker/graphite/query.hh"
 *  @brief Query compiling/generation.
 *
 *  This class compiles a query for further uses, generating
 *  the query fast.
 */
class query : public http_tsdb::line_protocol_query {
 public:
  query(std::string const& naming_scheme,
        std::string const& escape_string,
        data_type type,
        const std::shared_ptr<spdlog::logger>& logger);
  ~query() = default;

  query(query const& other) = delete;
  query& operator=(query const& other) = delete;

  std::string append_metric(storage::pb_metric const& me) const override;
  std::string append_status(storage::pb_status const& st) const override;

 private:
  // Used for generation.
  std::string _escape_string;
  void _escape(const std::string& str, std::ostream& is) const;
};

}  // namespace com::centreon::broker::graphite

#endif  // !CCB_GRAPHITE_QUERY_HH
