/**
 * Copyright 2022 Centreon
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

#ifndef CCB_EXCEPTIONS_DEPRECATED_HH
#define CCB_EXCEPTIONS_DEPRECATED_HH

#include "com/centreon/exceptions/msg_fmt.hh"

namespace com::centreon::broker {

namespace exceptions {
/**
 *  @class deprecated deprecated.hh
 * "com/centreon/broker/exceptions/deprecated.hh"
 *  @brief Shutdown exception class.
 *
 *  This exception is thrown when someone attemps to read from a
 *  stream that has been deprecated.
 */
class deprecated : public com::centreon::exceptions::msg_fmt {
 public:
  template <typename... Args>
  explicit deprecated(std::string const& str, const Args&... args)
      : msg_fmt(str, args...) {}
  deprecated() = delete;
  ~deprecated() noexcept = default;
  deprecated& operator=(const deprecated&) = delete;
};
}  // namespace exceptions

}  // namespace com::centreon::broker

#endif  // !CCB_EXCEPTIONS_DEPRECATED_HH
