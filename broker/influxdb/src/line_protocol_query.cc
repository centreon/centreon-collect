/**
 * Copyright 2015-2014 Centreon
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

#include "com/centreon/broker/influxdb/line_protocol_query.hh"
#include "com/centreon/broker/http_tsdb/line_protocol_query.hh"
#include "com/centreon/broker/misc/string.hh"
#include "com/centreon/exceptions/msg_fmt.hh"
#include "common/log_v2/log_v2.hh"

using namespace com::centreon::broker;
using namespace com::centreon::broker::influxdb;
using namespace com::centreon::exceptions;
using log_v2 = com::centreon::common::log_v2::log_v2;

/**
 *  Constructor.
 *
 *  @param[in] timeseries  Name of the time-series.
 *  @param[in] columns     Columns to add in the query.
 *  @param[in] type        Query type (metric or status).
 *  @param[in] cache       Macro cache.
 */
line_protocol_query::line_protocol_query(
    std::string const& timeseries,
    const std::string allowed_macros,
    std::vector<http_tsdb::column> const& columns,
    data_type type,
    const std::shared_ptr<spdlog::logger>& logger)
    : http_tsdb::line_protocol_query(type, logger) {
  // Following implementation is based on
  // https://docs.influxdata.com/influxdb/v1.2/write_protocols/line_protocol_tutorial/
  // The base format is <measurement>,<tag_set> <field_set> <timestamp>.
  // The tricky part is that each component as a different escaping
  // scheme.

  // measurement
  _compile_scheme(allowed_macros, timeseries, &escape_measurement);

  // tag_set
  for (std::vector<http_tsdb::column>::const_iterator it(columns.begin()),
       end(columns.end());
       it != end; ++it)
    if (it->is_tag()) {
      // comma
      _append_compiled_string(",");
      // tag_name
      _compile_scheme(allowed_macros, it->get_name(), &escape_key);
      // equal sign
      _append_compiled_string("=");
      // tag_value
      _compile_scheme(allowed_macros, it->get_value(), &escape_key);
    }

  // space
  _append_compiled_string(" ");

  // field_set
  bool first(true);
  for (std::vector<http_tsdb::column>::const_iterator it(columns.begin()),
       end(columns.end());
       it != end; ++it)
    if (!it->is_tag()) {
      if (first)
        first = false;
      else
        _append_compiled_string(",");

      // field_key
      _compile_scheme(allowed_macros, it->get_name(), &escape_key);
      // equal sign
      _append_compiled_string("=");
      // field value
      if (it->get_type() == http_tsdb::column::number)
        _compile_scheme(allowed_macros, it->get_value(), nullptr);
      else if (it->get_type() == http_tsdb::column::string)
        _compile_scheme(allowed_macros, it->get_value(), &escape_value);
    }
  if (!first)
    _append_compiled_string(" ");

  // timestamp
  _compile_scheme(allowed_macros, "$TIME$", nullptr);
  _append_compiled_string("\n");
}

/**
 *  Escape a key.
 *
 *  @param[in] str  String to escape.
 *
 */
void line_protocol_query::escape_key(std::string const& str, std::ostream& is) {
  std::string ret(str);
  ::com::centreon::broker::misc::string::replace(ret, ",", "\\,");
  ::com::centreon::broker::misc::string::replace(ret, "=", "\\=");
  ::com::centreon::broker::misc::string::replace(ret, " ", "\\ ");
  is << ret;
}

/**
 *  Escape a measurement.
 *
 *  @param[in] str  String to escape.
 *
 */
void line_protocol_query::escape_measurement(std::string const& str,
                                             std::ostream& is) {
  std::string ret(str);
  ::com::centreon::broker::misc::string::replace(ret, ",", "\\,");
  ::com::centreon::broker::misc::string::replace(ret, " ", "\\ ");
  is << ret;
}

/**
 *  Escape a value.
 *
 *  @param[in] str  String to escape.
 *
 */
void line_protocol_query::escape_value(std::string const& str,
                                       std::ostream& is) {
  std::string ret(str);
  ::com::centreon::broker::misc::string::replace(ret, "\"", "\\\"");
  ret.insert(0, "\"");
  ret.append("\"");
  is << ret;
}
