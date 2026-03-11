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

#include "com/centreon/broker/graphite/query.hh"
#include <absl/strings/str_cat.h>
#include "com/centreon/broker/http_tsdb/line_protocol_query.hh"
#include "com/centreon/broker/misc/string.hh"
#include "com/centreon/exceptions/msg_fmt.hh"
#include "common/log_v2/log_v2.hh"

using namespace com::centreon::broker;
using namespace com::centreon::exceptions;
using namespace com::centreon::broker::graphite;
using log_v2 = com::centreon::common::log_v2::log_v2;

/**
 *  Constructor.
 *
 *  @param[in] naming_scheme  The naming scheme to use.
 *  @param[in] escape_string  String used to escape special chars
 *                            (especially dot).
 *  @param[in] type           The type of the query: metric or status.
 *  @param[in] cache          The macro cache.
 */
query::query(std::string const& naming_scheme,
             std::string const& escape_string,
             data_type type,
             const std::shared_ptr<spdlog::logger>& logger)
    : http_tsdb::line_protocol_query(type, logger),
      _escape_string(escape_string) {
  _compile_scheme(
      "", naming_scheme,
      [this](const std::string& str, std::ostream& is) { _escape(str, is); },
      false);
}

std::string query::append_metric(storage::pb_metric const& me) const {
  std::string ret;
  if (http_tsdb::line_protocol_query::append_metric(me, ret)) {
    misc::string::replace(ret, " ", "_");
    absl::StrAppend(&ret, " ", me.obj().value(), " ", me.obj().time(), "\n");
  }
  return ret;
}

std::string query::append_status(storage::pb_status const& st) const {
  std::string ret;
  if (http_tsdb::line_protocol_query::append_status(st, ret)) {
    misc::string::replace(ret, " ", "_");
    absl::StrAppend(&ret, " ", st.obj().state(), " ", st.obj().time(), "\n");
  }
  return ret;
}

// /**
//  *  Generate the query for a metric.
//  *
//  *  @param[in] me  The metric.
//  *
//  *  @return  The query for a metric.
//  */
// std::string query::generate_metric(storage::pb_metric const& me) {
//   if (_type != metric)
//     throw msg_fmt(
//         "graphite: attempt to generate metric"
//         " with a query of the bad type");
//   _naming_scheme_index = 0;
//   std::ostringstream iss;
//   std::ostringstream tmp;
//   try {
//     for (std::vector<void (query::*)(io::data const&,
//                                      std::ostream&)>::const_iterator
//              it(_compiled_getters.begin()),
//          end(_compiled_getters.end());
//          it != end; ++it) {
//       (this->**it)(me, tmp);
//       std::string escaped = tmp.str();
//       misc::string::replace(escaped, " ", "_");
//       iss << escaped;
//       tmp.str("");
//     }
//   } catch (const std::exception& e) {
//     auto logger = log_v2::instance().get(log_v2::GRAPHITE);
//     logger->error("graphite: couldn't generate query for metric {}: {}",
//                   me.obj().metric_id(), e.what());
//     return "";
//   }

//   iss << (" ") << me.obj().value() << " " << me.obj().time() << "\n";

//   return iss.str();
// }

// /**
//  *  Generate the query for a status.
//  *
//  *  @param[in] st  The status.
//  *
//  *  @return  The query for a status.
//  */
// std::string query::generate_status(storage::pb_status const& st) {
//   if (_type != status)
//     throw msg_fmt(
//         "graphite: attempt to generate status"
//         " with a query of the bad type");
//   _naming_scheme_index = 0;
//   std::ostringstream iss;
//   std::ostringstream tmp;
//   try {
//     for (std::vector<void (query::*)(io::data const&,
//                                      std::ostream&)>::const_iterator
//              it(_compiled_getters.begin()),
//          end(_compiled_getters.end());
//          it != end; ++it) {
//       (this->**it)(st, tmp);
//       std::string escaped = tmp.str();
//       misc::string::replace(escaped, " ", "_");
//       iss << escaped;
//       tmp.str("");
//     }
//   } catch (std::exception const& e) {
//     auto logger = log_v2::instance().get(log_v2::GRAPHITE);
//     logger->error("graphite: couldn't generate query for status {}: {}",
//                   st.obj().index_id(), e.what());
//     return "";
//   }

//   iss << (" ") << st.obj().state() << " " << st.obj().time() << "\n";

//   return iss.str();
// }

/**
 *  Compile a naming scheme.
 *
 *  @param[in] naming_scheme  The naming scheme to compile.
 *  @param[in] type           The type of this query.
 */
// void query::_compile_naming_scheme(std::string const& naming_scheme,
//                                    data_type type) {
//   (void)type;
//   size_t found_macro = 0;
//   size_t end_macro = 0;

//   while ((found_macro = naming_scheme.find_first_of('$', found_macro)) !=
//          std::string::npos) {
//     std::string substr =
//         naming_scheme.substr(end_macro, found_macro - end_macro);
//     if (!substr.empty()) {
//       _compiled_naming_scheme.push_back(substr);
//       _compiled_getters.push_back(&query::_get_string);
//     }

//     if ((end_macro = naming_scheme.find_first_of('$', found_macro + 1)) ==
//         std::string::npos)
//       throw msg_fmt(
//           "graphite: can't compile query, opened macro not closed: '{}'",
//           naming_scheme.substr(found_macro));

//     std::string macro{
//         naming_scheme.substr(found_macro, end_macro + 1 - found_macro)};
//     if (macro == "$$")
//       _compiled_getters.push_back(&query::_get_dollar_sign);
//     if (macro == "$METRICID$") {
//       _throw_on_invalid(metric);
//       _compiled_getters.push_back(&query::_get_metric_id);
//     } else if (macro == "$INSTANCE$")
//       _compiled_getters.push_back(&query::_get_instance);
//     else if (macro == "$INSTANCEID$")
//       _compiled_getters.push_back(
//           &query::_get_member<unsigned int, io::data, &io::data::source_id>);
//     else if (macro == "$HOST$")
//       _compiled_getters.push_back(&query::_get_host);
//     else if (macro == "$HOSTID$")
//       _compiled_getters.push_back(&query::_get_host_id);
//     else if (macro == "$SERVICE$")
//       _compiled_getters.push_back(&query::_get_service);
//     else if (macro == "$SERVICEID$")
//       _compiled_getters.push_back(&query::_get_service_id);
//     else if (macro == "$METRIC$") {
//       _throw_on_invalid(metric);
//       _compiled_getters.push_back(&query::_get_metric_name);
//     } else if (macro == "$INDEXID$") {
//       _compiled_getters.push_back(&query::_get_index_id);
//     } else {
//       auto logger = log_v2::instance().get(log_v2::GRAPHITE);
//       logger->info("graphite: unknown macro '{}': ignoring it", macro);
//     }
//     found_macro = end_macro = end_macro + 1;
//   }
//   std::string substr = naming_scheme.substr(end_macro, found_macro -
//   end_macro); if (!substr.empty()) {
//     _compiled_naming_scheme.push_back(substr);
//     _compiled_getters.push_back(&query::_get_string);
//   }
// }

/**
 *  Escape data string.
 *
 *  @param[in] str  Base string.
 *
 *  @return Escaped string.
 */
void query::_escape(const std::string& str, std::ostream& is) const {
  std::string retval{str};
  size_t pos = retval.find('.');

  while (pos != std::string::npos) {
    retval.replace(pos, 1, _escape_string);
    pos = retval.find('.', pos + _escape_string.size());
  }
  is << retval;
}

/**
 *  Throw on invalid macro type.
 *
 *  @param[in] macro_type  The macro type;
 */
// void query::_throw_on_invalid(data_type macro_type) {
//   if (macro_type != _type)
//     throw msg_fmt("graphite: macro of invalid type");
// }
