/*
** Copyright 2014 Centreon
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

#ifndef CC_EXCEPTIONS_INTERRUPTION_HH
#define CC_EXCEPTIONS_INTERRUPTION_HH

#include "com/centreon/exceptions/msg_fmt.hh"
#include "com/centreon/namespace.hh"

CC_BEGIN()

namespace exceptions {
/**
 *  @class interruption interruption.hh
 *"com/centreon/exceptions/interruption.hh"
 *  @brief Exception signaling an interruption in processing.
 *
 *  Some operation that was in progress was interrupted but did not
 *  fail. This is mostly used to warn users of an errno of EINTR
 *  during a syscall.
 */
class interruption : public msg_fmt {
 public:
  interruption();
  interruption(char const* file, char const* function, int line);
  interruption(interruption const& other);
  ~interruption() noexcept {}
  interruption& operator=(interruption const& other);
  /*template <typename T>
  interruption& operator<<(T t) {
    basic::operator<<(t);
    return (*this);*/
  template <typename... Args>
  explicit interruption(std::string const& str, const Args&... args)
      : msg_fmt(fmt::format(str, args...)) {}
};
}

CC_END()

#if defined(__GNUC__)
#define FUNCTION __PRETTY_FUNCTION__
#elif defined(_MSC_VER)
#define FUNCTION __FUNCSIG__
#else
#define FUNCTION __func__
#endif  // GCC, Visual or other.

#ifndef NDEBUG
/*#define interruption_error() \
  com::centreon::exceptions::basic(__FILE__, FUNCTION, __LINE__)
#else
#define interruption_error() com::centreon::exceptions::basic()*/

#define interruption_error(format, ...) \
  com::centreon::exceptions::interruption(format, __VA_ARGS__)
#define interruption_error_1(format) \
  com::centreon::exceptions::interruption(format)
#else
#define interruption_error(format, ...)    \
  com::centreon::exceptions::interruption( \
      "[{}:{}:{}] " format, __FILE__, __func__, __LINE__, __VA_ARGS__)
#define interruption_error_1(format)       \
  com::centreon::exceptions::interruption( \
      "[{}:{}:{}] {}", __FILE__, __func__, __LINE__, format)

#endif  // !NDEBUG

#endif  // !CC_EXCEPTIONS_INTERRUPTION_HH
