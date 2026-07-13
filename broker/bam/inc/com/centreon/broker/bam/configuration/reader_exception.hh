/**
 * Copyright 2014, 2022-2024 Centreon
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

#ifndef CCB_CONFIGURATION_READER_EXCEPTION_HH
#define CCB_CONFIGURATION_READER_EXCEPTION_HH

#include "com/centreon/broker/exceptions/config.hh"

namespace com::centreon::broker {

namespace bam {
namespace configuration {
/**
 *  @class reader_exception reader_exception.hh
 * "com/centreon/broker/bam/configuration/reader_exception.hh"
 *  @brief Exception thrown when the reader fails to read from a database
 *
 *  Reader_exception.
 */
class reader_exception : public exceptions::config {
  unsigned _ba_id;

 public:
  reader_exception() = delete;

  template <typename... Args>
  explicit reader_exception(std::string const& str, const Args&... args)
      : exceptions::config(str, args...), _ba_id(0) {}

  template <typename... Args>
  explicit reader_exception(unsigned ba_id,
                            std::string const& str,
                            const Args&... args)
      : exceptions::config(str, args...), _ba_id(ba_id) {}

  reader_exception(const reader_exception&);
  ~reader_exception() noexcept {};

  unsigned ba_id() const { return _ba_id; }
};
}  // namespace configuration
}  // namespace bam

}  // namespace com::centreon::broker

#endif  // !CCB_CONFIGURATION_READER_EXCEPTION_HH
