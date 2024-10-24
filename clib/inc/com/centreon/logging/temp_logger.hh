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

#ifndef CC_LOGGING_TEMP_LOGGER_HH
#define CC_LOGGING_TEMP_LOGGER_HH

#include "com/centreon/logging/engine.hh"
#include "com/centreon/misc/stringifier.hh"

namespace com::centreon {

namespace logging {
struct setprecision {
  setprecision(int val = -1) : precision(val) {}
  int precision;
};

/**
 *  @class temp_logger temp_logger.hh "com/centreon/logging/temp_logger.hh"
 *  @brief Log messages.
 *
 *  Used to buffering log messages before writing them into backends.
 */
class temp_logger {
 public:
  temp_logger(unsigned long long types, unsigned int verbose) noexcept;
  temp_logger(temp_logger const& right);
  virtual ~temp_logger() throw();
  temp_logger& operator=(temp_logger const& right);
  temp_logger& operator<<(setprecision const& obj) noexcept;
  template <typename T>
  temp_logger& operator<<(T obj) noexcept {
    _buffer << obj;
    return (*this);
  }

 private:
  temp_logger& _internal_copy(temp_logger const& right);

  misc::stringifier _buffer;
  engine& _engine;
  unsigned long long _type;
  unsigned int _verbose;
};
}  // namespace logging

}

#endif  // !CC_LOGGING_TEMP_LOGGER_HH
