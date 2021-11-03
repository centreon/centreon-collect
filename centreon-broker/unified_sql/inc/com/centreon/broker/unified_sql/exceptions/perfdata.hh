/*
** Copyright 2011-2013 Centreon
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

#ifndef CCB_UNIFIED_SQL_EXCEPTIONS_PERFDATA_HH
#define CCB_UNIFIED_SQL_EXCEPTIONS_PERFDATA_HH

#include "com/centreon/broker/namespace.hh"
#include "com/centreon/exceptions/msg_fmt.hh"

using namespace com::centreon::exceptions;

CCB_BEGIN()

namespace unified_sql {
namespace exceptions {
/**
 *  @class perfdata perfdata.hh
 * "com/centreon/broker/unified_sql/exceptions/perfdata.hh"
 *  @brief Perfdata exception.
 *
 *  Exception thrown when handling performance data.
 */
class perfdata : public msg_fmt {
 public:
  template <typename... Args>
  explicit perfdata(std::string const& str, const Args&... args)
      : msg_fmt(str, args...) {}
  perfdata() = delete;
  ~perfdata() noexcept {}
  perfdata& operator=(const perfdata&) = delete;
};
}  // namespace exceptions
}  // namespace unified_sql

CCB_END()

#endif  // !CCB_UNIFIED_SQL_EXCEPTIONS_PERFDATA_HH
