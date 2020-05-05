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

#ifndef CCCP_CHECKS_LISTENER_HH
#define CCCP_CHECKS_LISTENER_HH

#include "com/centreon/connector/perl/checks/result.hh"
#include "com/centreon/connector/perl/namespace.hh"

CCCP_BEGIN()

namespace checks {
/**
 *  @class listener listener.hh "com/centreon/connector/perl/checks/listener.hh"
 *  @brief Check listener.
 *
 *  Listen check events.
 */
class listener {
 public:
  listener() = default;
  listener(listener const& l) = delete;
  virtual ~listener() = default;
  listener& operator=(listener const& l) = delete;
  virtual void on_result(result const& result) = 0;
};
}  // namespace checks

CCCP_END()

#endif  // !CCCP_CHECKS_LISTENER_HH
