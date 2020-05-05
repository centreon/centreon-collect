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

#ifndef CCCS_SESSIONS_LISTENER_HH
#define CCCS_SESSIONS_LISTENER_HH

#include "com/centreon/connector/ssh/namespace.hh"

CCCS_BEGIN()

namespace sessions {
// Forward declaration.
class session;

/**
 *  @class listener listener.hh
 * "com/centreon/connector/ssh/sessions/listener.hh"
 *  @brief Session listener.
 *
 *  Listen session events.
 */
class listener {
 public:
  listener() = default;
  virtual ~listener() = default;
  listener(listener const& l) = delete;
  listener& operator=(listener const& l) = delete;
  virtual void on_available(session& s) = 0;
  virtual void on_close(session& s) = 0;
  virtual void on_connected(session& s) = 0;
};
}  // namespace sessions

CCCS_END()

#endif  // !CCCS_SESSIONS_LISTENER_HH
