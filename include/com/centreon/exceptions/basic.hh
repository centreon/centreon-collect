/*
** Copyright 2011-2014 Centreon
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

#ifndef CC_EXCEPTIONS_BASIC_HH
#define CC_EXCEPTIONS_BASIC_HH

#include "com/centreon/exceptions/msg_fmt.hh"
#include "com/centreon/namespace.hh"

CC_BEGIN()

namespace exceptions {
/**
 *  @class basic basic.hh "com/centreon/exceptions/basic.hh"
 *  @brief Base exception class.
 *
 *  Simple exception class containing an basic error message.
 */
class basic : public msg_fmt {
 public:
  template <typename... Args>
  explicit basic(std::string const& str, const Args&... args)
      : msg_fmt(fmt::format(str, args...)) {}

  basic() = delete;
  ~basic() noexcept {}
  basic& operator=(const basic&) = delete;
};

}  // namespace exceptions

CC_END()

#ifdef NDEBUG
#define basic_error(format, ...) \
  com::centreon::exceptions::basic(format, __VA_ARGS__)
#define basic_error_1(format) com::centreon::exceptions::basic(format)
#else
#define basic_error(format, ...)    \
  com::centreon::exceptions::basic( \
      "[{}:{}:{}] " format, __FILE__, __func__, __LINE__, __VA_ARGS__)
#define basic_error_1(format)       \
  com::centreon::exceptions::basic( \
      "[{}:{}:{}] {}", __FILE__, __func__, __LINE__, format)
#endif  // !NDEBUG

#endif  // !CC_EXCEPTIONS_BASIC_HH