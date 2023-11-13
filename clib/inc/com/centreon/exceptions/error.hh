/**
 * Copyright 2023 Centreon
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

#ifndef CCB_EXCEPTIONS_ERROR_HH
#define CCB_EXCEPTIONS_ERROR_HH

#include "com/centreon/exceptions/msg_fmt.hh"

namespace com::centreon::exceptions {
/**
 *  @class error error.hh "com/centreon/exceptions/error.hh"
 *  @brief Shutdown exception class.
 *
 *  This exception is thrown when someone attemps to read from a
 *  stream that has been error.
 */
class error : public com::centreon::exceptions::msg_fmt {
 public:
  template <typename... Args>
  explicit error(std::string const& str, const Args&... args)
      : msg_fmt(str, args...) {}
  error() = delete;
  ~error() noexcept {}
  error& operator=(const error&) = delete;
};
}  // namespace com::centreon::exceptions

#endif  // !CCB_EXCEPTIONS_ERROR_HH
