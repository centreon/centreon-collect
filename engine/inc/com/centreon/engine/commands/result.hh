/*
 * Copyright 2011 - 2020 Centreon (https://www.centreon.com/)
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * For more information : contact@centreon.com
 *
 */
#ifndef CCE_COMMANDS_RESULT_HH
#define CCE_COMMANDS_RESULT_HH

#include "com/centreon/process.hh"
#include "com/centreon/timestamp.hh"

namespace com::centreon::engine {

class check_result;

namespace commands {

/**
 *  @class result result.hh
 *  @brief Result contain the result of execution process.
 *
 *  Result contain the result of execution process (output, retvalue,
 *  execution time).
 */
class result {
  void _internal_copy(result const& right);

 public:
  uint64_t command_id;
  timestamp end_time;
  int exit_code;
  process::status exit_status;
  timestamp start_time;
  std::string output;

  result();
  result(result const& right);
  result(const check_result& check_res);
  ~result() noexcept;
  result& operator=(result const& right);
  bool operator==(result const& right) const noexcept;
  bool operator!=(result const& right) const noexcept;
};

std::ostream& operator<<(std::ostream& s, const result& to_dump);

}  // namespace commands

}

namespace fmt {
template <>
struct formatter<com::centreon::engine::commands::result> : ostream_formatter {
};
}  // namespace fmt

#endif  // !CCE_COMMANDS_RESULT_HH
