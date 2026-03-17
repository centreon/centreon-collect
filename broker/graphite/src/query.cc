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
      false, false);
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
