/*
** Copyright 2011,2017,2020 Centreon
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

#ifndef CCB_EXCEPTIONS_CONNECTION_CLOSED_HH
#define CCB_EXCEPTIONS_CONNECTION_CLOSED_HH

#include "com/centreon/exceptions/msg_fmt.hh"

namespace com::centreon::broker {

namespace exceptions {
/**
 *  @class connection_closed connection_closed.hh
 * "com/centreon/broker/exceptions/connection_closed.hh"
 *  @brief connection_closed exception class.
 *
 *  This exception is thrown when someone attemps to read from a
 *  stream that has been connection_closed.
 */
class connection_closed : public com::centreon::exceptions::msg_fmt {
 public:
  template <typename... Args>
  explicit connection_closed(std::string const& str, const Args&... args)
      : msg_fmt(str, args...) {}
  connection_closed() = delete;
  ~connection_closed() noexcept {}
  connection_closed& operator=(const connection_closed&) = delete;
};
}  // namespace exceptions

}  // namespace com::centreon::broker

#endif  // !CCB_EXCEPTIONS_CONNECTION_CLOSED_HH
