/**
 * Copyright 2015-2017 Centreon
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

#include "com/centreon/broker/victoria_metrics/line_protocol_query.hh"
#include "com/centreon/broker/http_tsdb/internal.hh"
#include "com/centreon/broker/misc/string.hh"
#include "com/centreon/exceptions/msg_fmt.hh"

using namespace com::centreon::broker;
using namespace com::centreon::broker::victoria_metrics;
using namespace com::centreon::exceptions;

/**
 *  Constructor.
 *
 *  @param[in] timeseries  Name of the time-series.
 *  @param[in] columns     Columns to add in the query.
 *  @param[in] type        Query type (metric or status).
 */
line_protocol_query::line_protocol_query(
    const std::string allowed_macros,
    std::vector<http_tsdb::column> const& columns,
    data_type type,
    const std::shared_ptr<spdlog::logger>& logger)
    : http_tsdb::line_protocol_query(type, logger) {
  bool have_field = false;
  // tag_set
  for (std::vector<http_tsdb::column>::const_iterator it(columns.begin()),
       end(columns.end());
       it != end; ++it) {
    if (it->is_tag()) {
      // comma
      _append_compiled_string(",");
      // tag_name
      _compile_scheme(allowed_macros, it->get_name(),
                      &line_protocol_query::escape_key, true);
      // equal sign
      _append_compiled_string("=");
      // tag_value
      _compile_scheme(allowed_macros, it->get_value(),
                      &line_protocol_query::escape_value, true);
    } else {
      have_field = true;
    }
  }

  if (have_field) {
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
        _compile_scheme(allowed_macros, it->get_name(),
                        &line_protocol_query::escape_key, true);
        // equal sign
        _append_compiled_string("=");
        // field value
        if (it->get_type() == http_tsdb::column::type::number)
          _compile_scheme(allowed_macros, it->get_value(), nullptr, true);
        else if (it->get_type() == http_tsdb::column::type::string)
          _compile_scheme(allowed_macros, it->get_value(),
                          &line_protocol_query::escape_value, true);
      }
  }
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
 *  Escape a value.
 *
 *  @param[in] str  String to escape.
 *
 *  @return Escaped string.
 */
void line_protocol_query::escape_value(std::string const& str,
                                       std::ostream& is) {
  for (const char c : str) {
    if (c == ',') {
      is << "\\,";
    } else if (c == '"') {
      is << "\\\"";
    } else if (c == ' ') {
      is << "\\ ";
    } else if (c == '\\') {
      is << "\\\\";
    } else {
      is << c;
    }
  }
}
