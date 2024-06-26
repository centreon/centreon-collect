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
#include "com/centreon/broker/misc/string.hh"
#include "com/centreon/exceptions/msg_fmt.hh"
#include "common/log_v2/log_v2.hh"

using namespace com::centreon::broker;
using namespace com::centreon::broker::influxdb;
using namespace com::centreon::exceptions;
using log_v2 = com::centreon::common::log_v2::log_v2;

/**
 *  Create an empty query.
 */
line_protocol_query::line_protocol_query()
    : _type(line_protocol_query::unknown), _cache(nullptr) {}

/**
 *  Constructor.
 *
 *  @param[in] timeseries  Name of the time-series.
 *  @param[in] columns     Columns to add in the query.
 *  @param[in] type        Query type (metric or status).
 *  @param[in] cache       Macro cache.
 */
line_protocol_query::line_protocol_query(std::string const& timeseries,
                                         std::vector<column> const& columns,
                                         data_type type,
                                         macro_cache const& cache)
    : _string_index{0}, _type{type}, _cache{&cache} {
  // Following implementation is based on
  // https://docs.influxdata.com/influxdb/v1.2/write_protocols/line_protocol_tutorial/
  // The base format is <measurement>,<tag_set> <field_set> <timestamp>.
  // The tricky part is that each component as a different escaping
  // scheme.

  // measurement
  _compiled_getters.clear();
  _compiled_strings.clear();
  _compile_scheme(timeseries, &line_protocol_query::escape_measurement);

  // tag_set
  for (std::vector<column>::const_iterator it(columns.begin()),
       end(columns.end());
       it != end; ++it)
    if (it->is_flag()) {
      // comma
      _append_compiled_string(",");
      // tag_name
      _compile_scheme(it->get_name(), &line_protocol_query::escape_key);
      // equal sign
      _append_compiled_string("=");
      // tag_value
      _compile_scheme(it->get_value(), &line_protocol_query::escape_key);
    }

  // space
  _append_compiled_string(" ");

  // field_set
  bool first(true);
  for (std::vector<column>::const_iterator it(columns.begin()),
       end(columns.end());
       it != end; ++it)
    if (!it->is_flag()) {
      if (first)
        first = false;
      else
        _append_compiled_string(",");

      // field_key
      _compile_scheme(it->get_name(), &line_protocol_query::escape_key);
      // equal sign
      _append_compiled_string("=");
      // field value
      if (it->get_type() == column::number)
        _compile_scheme(it->get_value(), nullptr);
      else if (it->get_type() == column::string)
        _compile_scheme(it->get_value(), &line_protocol_query::escape_value);
    }
  if (!first)
    _append_compiled_string(" ");

  // timestamp
  _compile_scheme("$TIME$", nullptr);
  _append_compiled_string("\n");
}

/**
 *  Assignment operator.
 *
 *  @param[in] other  The object to copy.
 *
 *  @return This object.
 */
line_protocol_query& line_protocol_query::operator=(
    line_protocol_query const& other) {
  if (this != &other) {
    _compiled_getters = other._compiled_getters;
    _compiled_strings = other._compiled_strings;
    _string_index = 0;
    _type = other._type;
    _cache = other._cache;
  }
  return *this;
}

/**
 *  Escape a key.
 *
 *  @param[in] str  String to escape.
 *
 *  @return Escaped string.
 */
std::string line_protocol_query::escape_key(std::string const& str) {
  std::string ret(str);
  ::com::centreon::broker::misc::string::replace(ret, ",", "\\,");
  ::com::centreon::broker::misc::string::replace(ret, "=", "\\=");
  ::com::centreon::broker::misc::string::replace(ret, " ", "\\ ");
  return ret;
}

/**
 *  Escape a measurement.
 *
 *  @param[in] str  String to escape.
 *
 *  @return Escaped string.
 */
std::string line_protocol_query::escape_measurement(std::string const& str) {
  std::string ret(str);
  ::com::centreon::broker::misc::string::replace(ret, ",", "\\,");
  ::com::centreon::broker::misc::string::replace(ret, " ", "\\ ");
  return ret;
}

/**
 *  Escape a value.
 *
 *  @param[in] str  String to escape.
 *
 *  @return Escaped string.
 */
std::string line_protocol_query::escape_value(std::string const& str) {
  std::string ret(str);
  ::com::centreon::broker::misc::string::replace(ret, "\"", "\\\"");
  ret.insert(0, "\"");
  ret.append("\"");
  return ret;
}

/**
 *  Generate the query for a metric.
 *
 *  @param[in] me  The metric.
 *
 *  @return  The query for a metric.
 */
std::string line_protocol_query::generate_metric(const storage::pb_metric& me) {
  if (_type != metric)
    throw msg_fmt(
        "influxdb: attempt to generate metric"
        " with a query of the bad type");
  _string_index = 0;
  std::ostringstream iss;
  try {
    for (std::vector<std::pair<data_getter, data_escaper> >::const_iterator
             it(_compiled_getters.begin()),
         end(_compiled_getters.end());
         it != end; ++it) {
      if (!it->second)
        (this->*(it->first))(me, iss);
      else {
        std::ostringstream escaped;
        (this->*(it->first))(me, escaped);
        iss << (this->*(it->second))(escaped.str());
      }
    }
  } catch (std::exception const& e) {
    auto logger = log_v2::instance().get(log_v2::INFLUXDB);
    logger->error("influxdb: could not generate query for metric {}: {}",
                  me.obj().metric_id(), e.what());
    return "";
  }
  return iss.str();
}

/**
 *  Generate the query for a status.
 *
 *  @param[in] st  The status.
 *
 *  @return  The query for a status.
 */
std::string line_protocol_query::generate_status(const storage::pb_status& st) {
  if (_type != status)
    throw msg_fmt(
        "influxdb: attempt to generate status"
        " with a query of the bad type");
  _string_index = 0;
  std::ostringstream iss;
  try {
    for (std::vector<std::pair<data_getter, data_escaper> >::const_iterator
             it(_compiled_getters.begin()),
         end(_compiled_getters.end());
         it != end; ++it) {
      if (!it->second)
        (this->*(it->first))(st, iss);
      else {
        std::ostringstream escaped;
        (this->*(it->first))(st, escaped);
        iss << (this->*(it->second))(escaped.str());
      }
    }
  } catch (std::exception const& e) {
    auto logger = log_v2::instance().get(log_v2::INFLUXDB);
    logger->error("influxdb: could not generate query for status {}: {}",
                  st.obj().index_id(), e.what());
    return "";
  }

  return iss.str();
}

/**
 *  Append a getter and its escaper to the list of compiled getters.
 *
 *  @param[in] getter   Data getter.
 *  @param[in] escaper  Data escaper.
 */
void line_protocol_query::_append_compiled_getter(
    line_protocol_query::data_getter getter,
    line_protocol_query::data_escaper escaper) {
  _compiled_getters.push_back(std::make_pair(getter, escaper));
}

/**
 *  Append a raw string to the list of compiled strings.
 *
 *  @param[in] str      String to append.
 *  @param[in] escaper  Data escaper.
 */
void line_protocol_query::_append_compiled_string(
    std::string const& str,
    line_protocol_query::data_escaper escaper) {
  _compiled_strings.push_back(str);
  _compiled_getters.push_back(
      std::make_pair(&line_protocol_query::_get_string, escaper));
}

/**
 *  Compile a scheme.
 *
 *  @param[in] scheme   The scheme to compile.
 *  @param[in] escaper  Escaper for the scheme.
 */
void line_protocol_query::_compile_scheme(
    std::string const& scheme,
    line_protocol_query::data_escaper escaper) {
  size_t found_macro(0);
  size_t end_macro(0);

  while ((found_macro = scheme.find_first_of('$', found_macro)) !=
         std::string::npos) {
    std::string substr(scheme.substr(end_macro, found_macro - end_macro));
    if (!substr.empty())
      _append_compiled_string(substr, escaper);

    if ((end_macro = scheme.find_first_of('$', found_macro + 1)) ==
        std::string::npos)
      throw msg_fmt(
          "influxdb: can't compile query, opened macro not closed: '{}'",
          scheme.substr(found_macro));

    std::string macro(scheme.substr(found_macro, end_macro + 1 - found_macro));
    if (macro == "$$")
      _append_compiled_getter(&line_protocol_query::_get_dollar_sign, escaper);
    if (macro == "$METRICID$") {
      _throw_on_invalid(metric);
      _append_compiled_getter(&line_protocol_query::_get_metric_id, escaper);
    } else if (macro == "$INSTANCE$")
      _append_compiled_getter(&line_protocol_query::_get_instance, escaper);
    else if (macro == "$INSTANCEID$")
      _append_compiled_getter(
          &line_protocol_query::_get_member<uint32_t, io::data,
                                            &io::data::source_id>,
          escaper);
    else if (macro == "$HOST$")
      _append_compiled_getter(&line_protocol_query::_get_host, escaper);
    else if (macro == "$HOSTID$")
      _append_compiled_getter(&line_protocol_query::_get_host_id, escaper);
    else if (macro == "$SERVICE$")
      _append_compiled_getter(&line_protocol_query::_get_service, escaper);
    else if (macro == "$SERVICEID$")
      _append_compiled_getter(&line_protocol_query::_get_service_id, escaper);
    else if (macro == "$METRIC$") {
      _throw_on_invalid(metric);
      _append_compiled_getter(&line_protocol_query::_get_metric_name, escaper);
    } else if (macro == "$INDEXID$")
      _append_compiled_getter(&line_protocol_query::_get_index_id, escaper);
    else if (macro == "$VALUE$") {
      if (_type == metric)
        _append_compiled_getter(&line_protocol_query::_get_metric_value,
                                escaper);
      else if (_type == status)
        _append_compiled_getter(&line_protocol_query::_get_status_state,
                                escaper);
    } else if (macro == "$TIME$") {
      if (_type == metric)
        _append_compiled_getter(&line_protocol_query::_get_metric_time,
                                escaper);
      else if (_type == status)
        _append_compiled_getter(&line_protocol_query::_get_status_time,
                                escaper);
    } else {
      auto logger = log_v2::instance().get(log_v2::INFLUXDB);
      logger->info("influxdb: unknown macro '{}': ignoring it", macro);
    }

    found_macro = end_macro = end_macro + 1;
  }
  std::string substr(scheme.substr(end_macro, found_macro - end_macro));
  if (!substr.empty())
    _append_compiled_string(substr, escaper);
}

/**
 *  Throw on invalid macro type.
 *
 *  @param[in] macro_type  The macro type;
 */
void line_protocol_query::_throw_on_invalid(data_type macro_type) {
  if (macro_type != _type)
    throw msg_fmt("influxdb: macro of invalid type");
}

/**
 *  Get a member of the data.
 *
 *  @param[in] d    The data.
 *  @param[out] is  The stream.
 */
template <typename T, typename U, T(U::*member)>
void line_protocol_query::_get_member(io::data const& d, std::ostream& is) {
  is << static_cast<U const&>(d).*member;
}

/**
 *  Get a string in the compiled naming scheme.
 *
 *  @param[in] d     The data, unused.
 *  @param[out] is   The stream.
 */
void line_protocol_query::_get_string(io::data const& d, std::ostream& is) {
  (void)d;
  is << _compiled_strings[_string_index++];
}

/**
 *  Get a dollar sign (for escape).
 *
 *  @param[in] d   Unused.
 *  @param[in] is  The stream.
 */
void line_protocol_query::_get_dollar_sign(io::data const& d,
                                           std::ostream& is) {
  (void)d;
  is << "$";
}
/**
 *  Get the status index id of a data, be it either metric or status.
 *
 *  @param[in] d  The data.
 *
 *  @return       The index id.
 */
uint64_t line_protocol_query::_get_index_id(io::data const& d) {
  if (_type == status)
    return static_cast<storage::pb_status const&>(d).obj().index_id();
  else
    return _cache
        ->get_metric_mapping(
            static_cast<storage::pb_metric const&>(d).obj().metric_id())
        .obj()
        .index_id();
}

/**
 *  Get the status index id of a data, be it either metric or status.
 *
 *  @param[in] d    The data.
 *  @param[out] is  The stream.
 */
void line_protocol_query::_get_index_id(io::data const& d, std::ostream& is) {
  is << _get_index_id(d);
}

/**
 *  Get the name of a host.
 *
 *  @param[in] d  The data.
 *  @param is     The stream.
 */
void line_protocol_query::_get_host(io::data const& d, std::ostream& is) {
  if (_type == status)
    is << _cache->get_host_name(
        static_cast<storage::pb_status const&>(d).obj().host_id());
  else
    is << _cache->get_host_name(
        static_cast<storage::pb_metric const&>(d).obj().host_id());
}

/**
 *  Get the id of a host.
 *
 *  @param[in] d  The data.
 *  @param is     The stream.
 */
void line_protocol_query::_get_host_id(io::data const& d, std::ostream& is) {
  if (_type == status)
    is << static_cast<storage::pb_status const&>(d).obj().host_id();
  else
    is << static_cast<storage::pb_metric const&>(d).obj().host_id();
}

/**
 *  Get the name of a service.
 *
 *  @param[in] d  The data.
 *  @param is     The stream.
 */
void line_protocol_query::_get_service(io::data const& d, std::ostream& is) {
  if (_type == status) {
    is << _cache->get_service_description(
        static_cast<storage::pb_status const&>(d).obj().host_id(),
        static_cast<storage::pb_status const&>(d).obj().service_id());
  } else {
    is << _cache->get_service_description(
        static_cast<storage::pb_metric const&>(d).obj().host_id(),
        static_cast<storage::pb_metric const&>(d).obj().service_id());
  }
}

/**
 *  Get the id of a service.
 *
 *  @param[in] d  The data.
 *  @param is     The stream.
 */
void line_protocol_query::_get_service_id(io::data const& d, std::ostream& is) {
  if (_type == status)
    is << static_cast<storage::pb_status const&>(d).obj().service_id();
  else
    is << static_cast<storage::pb_metric const&>(d).obj().service_id();
}

/**
 *  Get the name of an instance.
 *
 *  @param[in] d  The data.
 *  @param is     The stream.
 */
void line_protocol_query::_get_instance(io::data const& d, std::ostream& is) {
  is << _cache->get_instance(d.source_id);
}

/**
 * @brief extract name of a metric
 *
 * @param d
 * @param is
 */
void line_protocol_query::_get_metric_name(io::data const& d,
                                           std::ostream& is) {
  is << static_cast<storage::pb_metric const&>(d).obj().name();
}

/**
 * @brief extract id of a metric
 *
 * @param d
 * @param is
 */
void line_protocol_query::_get_metric_id(io::data const& d, std::ostream& is) {
  is << static_cast<storage::pb_metric const&>(d).obj().metric_id();
}

/**
 * @brief extract name of a value
 *
 * @param d
 * @param is
 */
void line_protocol_query::_get_metric_value(io::data const& d,
                                            std::ostream& is) {
  is << static_cast<storage::pb_metric const&>(d).obj().value();
}

/**
 * @brief extract time of a metric
 *
 * @param d
 * @param is
 */
void line_protocol_query::_get_metric_time(io::data const& d,
                                           std::ostream& is) {
  is << static_cast<storage::pb_metric const&>(d).obj().time();
}

/**
 * @brief extract state of a status
 *
 * @param d
 * @param is
 */
void line_protocol_query::_get_status_state(io::data const& d,
                                            std::ostream& is) {
  is << static_cast<storage::pb_status const&>(d).obj().state();
}

/**
 * @brief extract time of a status
 *
 * @param d
 * @param is
 */
void line_protocol_query::_get_status_time(io::data const& d,
                                           std::ostream& is) {
  is << static_cast<storage::pb_status const&>(d).obj().time();
}
