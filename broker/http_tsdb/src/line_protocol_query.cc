/*
** Copyright 2015-2017 Centreon
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

#include "com/centreon/broker/http_tsdb/line_protocol_query.hh"
#include "broker/core/misc/string.hh"
#include "com/centreon/broker/cache/global_cache.hh"
#include "com/centreon/broker/http_tsdb/internal.hh"
#include "com/centreon/exceptions/msg_fmt.hh"

using namespace com::centreon::broker;
using namespace com::centreon::broker::http_tsdb;
using namespace com::centreon::exceptions;

/**
 *  Create an empty query.
 */
line_protocol_query::line_protocol_query() : _type(data_type::unknown) {}

/**
 *  Constructor.
 *
 *  @param[in] timeseries  Name of the time-series.
 *  @param[in] columns     Columns to add in the query.
 *  @param[in] type        Query type (metric or status).
 */
line_protocol_query::line_protocol_query(
    const std::string allowed_macros,
    std::vector<column> const& columns,
    data_type type,
    const std::shared_ptr<spdlog::logger>& logger)
    : _type{type}, _logger(logger) {
  // measurement
  _compiled_getters.clear();
  _compiled_strings.clear();

  bool have_field = false;
  // tag_set
  for (std::vector<column>::const_iterator it(columns.begin()),
       end(columns.end());
       it != end; ++it) {
    if (it->is_tag()) {
      // comma
      _append_compiled_string(",");
      // tag_name
      _compile_scheme(allowed_macros, it->get_name(),
                      &line_protocol_query::escape_key);
      // equal sign
      _append_compiled_string("=");
      // tag_value
      _compile_scheme(allowed_macros, it->get_value(),
                      &line_protocol_query::escape_value);
    } else {
      have_field = true;
    }
  }

  if (have_field) {
    // space
    _append_compiled_string(" ");

    // field_set
    bool first(true);
    for (std::vector<column>::const_iterator it(columns.begin()),
         end(columns.end());
         it != end; ++it)
      if (!it->is_tag()) {
        if (first)
          first = false;
        else
          _append_compiled_string(",");

        // field_key
        _compile_scheme(allowed_macros, it->get_name(),
                        &line_protocol_query::escape_key);
        // equal sign
        _append_compiled_string("=");
        // field value
        if (it->get_type() == column::type::number)
          _compile_scheme(allowed_macros, it->get_value(), nullptr);
        else if (it->get_type() == column::type::string)
          _compile_scheme(allowed_macros, it->get_value(),
                          &line_protocol_query::escape_value);
      }
  }
}

/**
 *  Escape a key.
 *
 *  @param[in] str  String to escape.
 *
 */
void line_protocol_query::escape_key(std::string const& str,
                                     std::ostream& is) const {
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
                                       std::ostream& is) const {
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

/**
 *  Generate the query for a metric.
 *
 *  @param[in] me  The metric.
 *
 */
void line_protocol_query::append_metric(storage::pb_metric const& me,
                                        std::string& request_body) const {
  unsigned string_index = 0;
  std::ostringstream iss;
  try {
    for (std::vector<std::pair<data_getter, data_escaper> >::const_iterator
             it(_compiled_getters.begin()),
         end(_compiled_getters.end());
         it != end; ++it) {
      if (!it->second)
        (this->*(it->first))(me, string_index, iss);
      else {
        std::ostringstream escaped;
        (this->*(it->first))(me, string_index, escaped);
        (this->*(it->second))(escaped.str(), iss);
      }
    }
  } catch (std::exception const& e) {
    SPDLOG_LOGGER_ERROR(_logger, "could not generate query for metric {}: {}",
                        me.obj().metric_id(), e.what());
    return;
  }
  request_body += iss.str();
}

/**
 *  Generate the query for a status.
 *
 *  @param[in] st  The status.
 *
 */
void line_protocol_query::append_status(storage::pb_status const& st,
                                        std::string& request_body) const {
  unsigned string_index = 0;
  std::ostringstream iss;
  try {
    for (std::vector<std::pair<data_getter, data_escaper> >::const_iterator
             it(_compiled_getters.begin()),
         end(_compiled_getters.end());
         it != end; ++it) {
      if (!it->second)
        (this->*(it->first))(st, string_index, iss);
      else {
        std::ostringstream escaped;
        (this->*(it->first))(st, string_index, escaped);
        (this->*(it->second))(escaped.str(), iss);
      }
    }
  } catch (std::exception const& e) {
    SPDLOG_LOGGER_ERROR(_logger, "could not generate query for status {}: {}",
                        st.obj().index_id(), e.what());
    return;
  }

  request_body += iss.str();
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
    const std::string allowed_macros,
    const std::string& scheme,
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
      throw msg_fmt("can't compile query, opened macro not closed: '{}'",
                    scheme.substr(found_macro));

    std::string macro(scheme.substr(found_macro, end_macro + 1 - found_macro));
    if (allowed_macros.find(macro) != std::string::npos) {
      if (macro == "$$")
        _append_compiled_getter(&line_protocol_query::_get_dollar_sign,
                                escaper);
      if (macro == "$METRICID$") {
        _throw_on_invalid(data_type::metric);
        _append_compiled_getter(&line_protocol_query::_get_metric_id, escaper);
      } else if (macro == "$INSTANCE$")
        _append_compiled_getter(&line_protocol_query::_get_instance, escaper);
      else if (macro == "$INSTANCEID$")
        _append_compiled_getter(
            &line_protocol_query::_get_member<uint32_t, io::data,
                                              &io::data::source_id>,
            nullptr);
      else if (macro == "$HOST$")
        _append_compiled_getter(&line_protocol_query::_get_host, escaper);
      else if (macro == "$HOSTID$")
        _append_compiled_getter(&line_protocol_query::_get_host_id, nullptr);
      else if (macro == "$HOSTGROUP$")
        _append_compiled_getter(&line_protocol_query::_get_host_group, escaper);
      else if (macro == "$SERVICE$")
        _append_compiled_getter(&line_protocol_query::_get_service, escaper);
      else if (macro == "$SERVICEID$")
        _append_compiled_getter(&line_protocol_query::_get_service_id, nullptr);
      else if (macro == "$RESOURCEID$")
        _append_compiled_getter(&line_protocol_query::_get_resource_id,
                                nullptr);
      else if (macro == "$SERVICE_GROUP$")
        _append_compiled_getter(&line_protocol_query::_get_service_group,
                                escaper);
      else if (macro == "$MIN$")
        _append_compiled_getter(&line_protocol_query::_get_min, nullptr);
      else if (macro == "$MAX$")
        _append_compiled_getter(&line_protocol_query::_get_max, nullptr);
      else if (macro == "$METRIC$") {
        _throw_on_invalid(data_type::metric);
        _append_compiled_getter(&line_protocol_query::_get_metric_name,
                                escaper);
      } else if (macro == "$INDEXID$")
        _append_compiled_getter(&line_protocol_query::_get_index_id, escaper);
      else if (macro == "$HOST_TAG_CAT_ID$")
        _append_compiled_getter(&line_protocol_query::_get_tag_host_cat_id,
                                escaper);
      else if (macro == "$HOST_TAG_GROUP_ID$")
        _append_compiled_getter(&line_protocol_query::_get_tag_host_group_id,
                                escaper);
      else if (macro == "$HOST_TAG_CAT_NAME$")
        _append_compiled_getter(&line_protocol_query::_get_tag_host_cat_name,
                                escaper);
      else if (macro == "$HOST_TAG_GROUP_NAME$")
        _append_compiled_getter(&line_protocol_query::_get_tag_host_group_name,
                                escaper);
      else if (macro == "$SERV_TAG_CAT_ID$")
        _append_compiled_getter(&line_protocol_query::_get_tag_serv_cat_id,
                                escaper);
      else if (macro == "$SERV_TAG_GROUP_ID$")
        _append_compiled_getter(&line_protocol_query::_get_tag_serv_group_id,
                                escaper);
      else if (macro == "$SERV_TAG_CAT_NAME$")
        _append_compiled_getter(&line_protocol_query::_get_tag_serv_cat_name,
                                escaper);
      else if (macro == "$SERV_TAG_GROUP_NAME$")
        _append_compiled_getter(&line_protocol_query::_get_tag_serv_group_name,
                                escaper);
      else if (macro == "$VALUE$") {
        if (_type == data_type::metric)
          _append_compiled_getter(&line_protocol_query::_get_metric_value,
                                  escaper);
        else if (_type == data_type::status)
          _append_compiled_getter(&line_protocol_query::_get_status_state,
                                  escaper);
      } else if (macro == "$TIME$") {
        if (_type == data_type::metric)
          _append_compiled_getter(&line_protocol_query::_get_metric_time,
                                  escaper);
        else if (_type == data_type::status)
          _append_compiled_getter(&line_protocol_query::_get_status_time,
                                  escaper);
      } else
        SPDLOG_LOGGER_INFO(_logger, "unknown macro '{}': ignoring it", macro);
    } else {
      SPDLOG_LOGGER_ERROR(_logger, "macro '{}' not allowed: ignoring it",
                          macro);
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
    throw msg_fmt("macro of invalid type");
}

/**
 *  Get a member of the data.
 *
 *  @param[in] d    The data.
 *  @param[out] is  The stream.
 */
template <typename T, typename U, T(U::*member)>
void line_protocol_query::_get_member(io::data const& d,
                                      unsigned& string_index,
                                      std::ostream& is) const {
  is << static_cast<U const&>(d).*member;
}

/**
 *  Get a string in the compiled naming scheme.
 *
 *  @param[in] d     The data, unused.
 *  @param[out] is   The stream.
 */
void line_protocol_query::_get_string(io::data const& d,
                                      unsigned& string_index,
                                      std::ostream& is) const {
  (void)d;
  is << _compiled_strings[string_index++];
}

/**
 *  Get a dollar sign (for escape).
 *
 *  @param[in] d   Unused.
 *  @param[in] is  The stream.
 */
void line_protocol_query::_get_dollar_sign(io::data const& d,
                                           unsigned& string_index,
                                           std::ostream& is) const {
  (void)d;
  is << "$";
}
/**
 *  Get the status index id of a data, be it either metric or status.
 *
 *  @param[in] d  The data.
 *  caution   cache::global_cache must be locked before usage
 *  @return       The index id.
 */
uint64_t line_protocol_query::_get_index_id(io::data const& d) const {
  const cache::metric_info* infos;
  switch (d.type()) {
    case storage::pb_status::static_type():
      return static_cast<storage::pb_status const&>(d).obj().index_id();
    case storage::pb_metric::static_type():
      infos = cache::global_cache::instance_ptr()->get_metric_info(
          static_cast<storage::pb_metric const&>(d).obj().metric_id());
      if (!infos) {
        SPDLOG_LOGGER_ERROR(
            _logger, "unknown metric {}",
            static_cast<storage::pb_metric const&>(d).obj().metric_id());
        return 0;
      } else {
        return infos->index_id;
      }
      break;
    default:
      SPDLOG_LOGGER_ERROR(_logger, "unknown type {}", d.type());
      return 0;
  }
}

/**
 *  Get the status index id of a data, be it either metric or status.
 *
 *  @param[in] d    The data.
 *  @param[out] is  The stream.
 */
void line_protocol_query::_get_index_id(io::data const& d,
                                        unsigned& string_index,
                                        std::ostream& is) const {
  cache::global_cache::lock l;
  is << _get_index_id(d);
}

/**
 *  Get the name of a host.
 *
 *  @param[in] d  The data.
 *  @param is     The stream.
 */
void line_protocol_query::_get_host(io::data const& d,
                                    unsigned& string_index,
                                    std::ostream& is) const {
  uint64_t host_id =
      d.type() == storage::pb_metric::static_type()
          ? static_cast<storage::pb_metric const&>(d).obj().host_id()
          : static_cast<storage::pb_status const&>(d).obj().host_id();
  cache::global_cache::lock l;
  const cache::resource_info* host_info =
      cache::global_cache::instance_ptr()->get_host(host_id);
  if (host_info) {
    is << host_info->name;
  }
}

/**
 *  Get the id of a host.
 *
 *  @param[in] d  The data.
 *  @param is     The stream.
 */
uint64_t line_protocol_query::_get_host_id(io::data const& d) const {
  if (d.type() == storage::pb_metric::static_type()) {
    return static_cast<storage::pb_metric const&>(d).obj().host_id();
  } else {
    return static_cast<storage::pb_status const&>(d).obj().host_id();
  }
}

/**
 *  Get the id of a host.
 *
 *  @param[in] d  The data.
 *  @param is     The stream.
 */
void line_protocol_query::_get_host_id(io::data const& d,
                                       unsigned& string_index,
                                       std::ostream& is) const {
  is << _get_host_id(d);
}

/**
 *  Get the name of a service.
 *
 *  @param[in] d  The data.
 *  @param is     The stream.
 */
void line_protocol_query::_get_service(io::data const& d,
                                       unsigned& string_index,
                                       std::ostream& is) const {
  cache::host_serv_pair host_serv = _get_service_id(d);
  cache::global_cache::lock l;
  const cache::resource_info* serv_info =
      cache::global_cache::instance_ptr()->get_service(host_serv.first,
                                                       host_serv.second);
  if (serv_info) {
    is << serv_info->name;
  }
}

/**
 *  Get the id of a service.
 *
 *  @param[in] d  The data.
 *  @param is     The stream.
 */
cache::host_serv_pair line_protocol_query::_get_service_id(
    io::data const& d) const {
  if (d.type() == storage::pb_metric::static_type()) {
    return {static_cast<storage::pb_metric const&>(d).obj().host_id(),
            static_cast<storage::pb_metric const&>(d).obj().service_id()};
  } else {
    return {static_cast<storage::pb_status const&>(d).obj().host_id(),
            static_cast<storage::pb_status const&>(d).obj().service_id()};
  }
}
/**
 *  Get the id of a service.
 *
 *  @param[in] d  The data.
 *  @param is     The stream.
 */
void line_protocol_query::_get_service_id(io::data const& d,
                                          unsigned& string_index,
                                          std::ostream& is) const {
  is << _get_service_id(d).second;
}

/**
 *  Get the name of an instance.
 *
 *  @param[in] d  The data.
 *  @param is     The stream.
 */
void line_protocol_query::_get_instance(io::data const& d,
                                        unsigned& string_index,
                                        std::ostream& is) const {
  cache::global_cache::lock l;
  const cache::string* instance_name =
      cache::global_cache::instance_ptr()->get_instance_name(d.source_id);
  if (instance_name) {
    is << *instance_name;
  }
}

/**
 * @brief add host group(s) of a metric or status to request
 *
 * @param d pb_status or pb_metric
 * @param tag_type
 * @param is
 */
void line_protocol_query::_get_host_group(io::data const& d,
                                          unsigned& string_index,
                                          std::ostream& is) const {
  cache::global_cache::lock l;
  cache::global_cache::instance_ptr()->append_host_group(_get_host_id(d), is);
}

/**
 * @brief add service group(s) of a metric or status to request
 *
 * @param d pb_status or pb_metric
 * @param tag_type
 * @param is
 */
void line_protocol_query::_get_service_group(io::data const& d,
                                             unsigned& string_index,
                                             std::ostream& is) const {
  cache::host_serv_pair host_serv = _get_service_id(d);
  cache::global_cache::lock l;
  cache::global_cache::instance_ptr()->append_service_group(
      host_serv.first, host_serv.second, is);
}

/**
 * @brief add min of a metric to request
 *
 * @param d pb_status or pb_metric
 * @param tag_type
 * @param is
 */
void line_protocol_query::_get_min(io::data const& d,
                                   unsigned& string_index,
                                   std::ostream& is) const {
  cache::global_cache::lock l;
  const cache::metric_info* infos = _get_metric_info(d);
  if (infos) {
    is << infos->min;
  }
}

/**
 * @brief add max of a metric to request
 *
 * @param d pb_status or pb_metric
 * @param tag_type
 * @param is
 */
void line_protocol_query::_get_max(io::data const& d,
                                   unsigned& string_index,
                                   std::ostream& is) const {
  cache::global_cache::lock l;
  const cache::metric_info* infos = _get_metric_info(d);
  if (infos) {
    is << infos->max;
  }
}

/**
 * @brief add ressource id of a serv to request
 *
 * @param d pb_status or pb_metric
 * @param tag_type
 * @param is
 */
void line_protocol_query::_get_resource_id(io::data const& d,
                                           unsigned& string_index,
                                           std::ostream& is) const {
  cache::host_serv_pair host_serv = _get_service_id(d);
  cache::global_cache::lock l;
  const cache::resource_info* serv_info =
      cache::global_cache::instance_ptr()->get_service(host_serv.first,
                                                       host_serv.second);
  if (serv_info) {
    is << serv_info->resource_id;
  }
}

/**
 * @brief find metric info fot a metric
 *     caution cache must be locked before usage
 *
 * @param d
 * @return const cache::metric_info*
 */
const cache::metric_info* line_protocol_query::_get_metric_info(
    io::data const& d) const {
  const cache::metric_info* infos = nullptr;
  if (storage::pb_metric::static_type()) {
    infos = cache::global_cache::instance_ptr()->get_metric_info(
        static_cast<storage::pb_metric const&>(d).obj().metric_id());
    if (!infos) {
      SPDLOG_LOGGER_ERROR(
          _logger, "unknown metric {}",
          static_cast<storage::pb_metric const&>(d).obj().metric_id());
    }
  } else {
    SPDLOG_LOGGER_ERROR(_logger, "_get_metric_info unknown type {}", d.type());
  }
  return infos;
}

/**
 * @brief add tag(s) id to request
 *
 * @param d pb_status or pb_metric
 * @param tag_type
 * @param is
 */
void line_protocol_query::_get_tag_host_id(io::data const& d,
                                           TagType tag_type,
                                           std::ostream& is) const {
  cache::global_cache::lock l;
  cache::global_cache::instance_ptr()->append_host_tag_id(_get_host_id(d),
                                                          tag_type, is);
}

/**
 * @brief add tag(s) name to request
 *
 * @param d pb_status or pb_metric
 * @param tag_type
 * @param is
 */
void line_protocol_query::_get_tag_host_name(io::data const& d,
                                             TagType tag_type,
                                             std::ostream& is) const {
  cache::global_cache::lock l;
  cache::global_cache::instance_ptr()->append_host_tag_name(_get_host_id(d),
                                                            tag_type, is);
}

/**
 * @brief add tag(s) id to request
 *
 * @param d pb_status or pb_metric
 * @param tag_type
 * @param is
 */
void line_protocol_query::_get_tag_serv_id(io::data const& d,
                                           TagType tag_type,
                                           std::ostream& is) const {
  cache::host_serv_pair host_serv = _get_service_id(d);
  cache::global_cache::lock l;
  cache::global_cache::instance_ptr()->append_serv_tag_id(
      host_serv.first, host_serv.second, tag_type, is);
}

/**
 * @brief add tag(s) name to request
 *
 * @param d pb_status or pb_metric
 * @param tag_type
 * @param is
 */
void line_protocol_query::_get_tag_serv_name(io::data const& d,
                                             TagType tag_type,
                                             std::ostream& is) const {
  cache::host_serv_pair host_serv = _get_service_id(d);
  cache::global_cache::lock l;
  cache::global_cache::instance_ptr()->append_serv_tag_name(
      host_serv.first, host_serv.second, tag_type, is);
}

/**
 * @brief extract name of a metric
 *
 * @param d
 * @param is
 */
void line_protocol_query::_get_metric_name(io::data const& d,
                                           unsigned& string_index,
                                           std::ostream& is) const {
  is << static_cast<storage::pb_metric const&>(d).obj().name();
}

/**
 * @brief extract id of a metric
 *
 * @param d
 * @param is
 */
void line_protocol_query::_get_metric_id(io::data const& d,
                                         unsigned& string_index,
                                         std::ostream& is) const {
  is << static_cast<storage::pb_metric const&>(d).obj().metric_id();
}

/**
 * @brief extract name of a value
 *
 * @param d
 * @param is
 */
void line_protocol_query::_get_metric_value(io::data const& d,
                                            unsigned& string_index,
                                            std::ostream& is) const {
  is << static_cast<storage::pb_metric const&>(d).obj().value();
}

/**
 * @brief extract time of a metric
 *
 * @param d
 * @param is
 */
void line_protocol_query::_get_metric_time(io::data const& d,
                                           unsigned& string_index,
                                           std::ostream& is) const {
  is << static_cast<storage::pb_metric const&>(d).obj().time();
}

/**
 * @brief extract state of a status
 *
 * @param d
 * @param is
 */
void line_protocol_query::_get_status_state(io::data const& d,
                                            unsigned& string_index,
                                            std::ostream& is) const {
  is << static_cast<storage::pb_status const&>(d).obj().state();
}

/**
 * @brief extract time of a status
 *
 * @param d
 * @param is
 */
void line_protocol_query::_get_status_time(io::data const& d,
                                           unsigned& string_index,
                                           std::ostream& is) const {
  is << static_cast<storage::pb_status const&>(d).obj().time();
}
