/*
** Copyright 2020 Centreon
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
#ifndef CC_EXCEPTIONS_ERROR_HH
#define CC_EXCEPTIONS_ERROR_HH

#include "com/centreon/exceptions/msg_fmt.hh"
#include "com/centreon/namespace.hh"

CC_BEGIN()

namespace exceptions {
/**
 *  @class error error.hh
 *  @brief Base exception class.
 *
 *  Simple exception class containing an error message and a flag to
 *  determine if the error that generated the exception was either fatal
 *  or not.
 */
class error : public msg_fmt {
 public:
  template <typename... Args>
  explicit error(std::string const& str, const Args&... args)
      : msg_fmt(fmt::format(str, args...)) {}

  ~error() noexcept {}
  error() = delete;
  error& operator=(const error&) = delete;
};

}  // namespace exceptions

CC_END()

#ifdef NDEBUG
#define engine_error(format, ...) \
  com::centreon::exceptions::error(format, __VA_ARGS__)
#define engine_error_1(format) com::centreon::exceptions::error(format)
#else
#define engine_error(format, ...)                                            \
  com::centreon::exceptions::error("[{}:{}:{}] " format, __FILE__, __func__, \
                                   __LINE__, __VA_ARGS__)
#define engine_error_1(format)                                          \
  com::centreon::exceptions::error("[{}:{}:{}] {}", __FILE__, __func__, \
                                   __LINE__, format)
#endif  // !NDEBUG

#endif  // !CC_EXCEPTIONS_ERROR_HH
