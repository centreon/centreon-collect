/*
 * Copyright 2011 - 2013, 2020 Centreon (https://www.centreon.com/)
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
#include "com/centreon/engine/commands/result.hh"
#include "com/centreon/engine/check_result.hh"

#include "com/centreon/timestamp.hh"

using namespace com::centreon;
using namespace com::centreon::engine::commands;

/**
 *  Constructor.
 */
result::result() : command_id(0), exit_code(0), exit_status(process::normal) {}

/**
 *  Copy constructor.
 *
 *  @param[in] right The copy class.
 */
result::result(result const& right) {
  _internal_copy(right);
}

result::result(const check_result& check_res)
    : command_id(0),
      end_time(check_res.get_finish_time()),
      exit_code(check_res.get_return_code()),
      exit_status(check_res.get_exited_ok() ? process::normal : process::crash),
      start_time(check_res.get_start_time()),
      output(check_res.get_output()) {}

/**
 *  Destructor.
 */
result::~result() noexcept {}

/**
 *  Default copy operator.
 *
 *  @param[in] right The copy class.
 *
 *  @return This object.
 */
result& result::operator=(result const& right) {
  if (this != &right)
    _internal_copy(right);
  return *this;
}

/**
 *  Compare two result.
 *
 *  @param[in] right The object to compare.
 *
 *  @return True if object have the same value.
 */
bool result::operator==(result const& right) const noexcept {
  return (command_id == right.command_id && exit_code == right.exit_code &&
          exit_status == right.exit_status && end_time == right.end_time &&
          start_time == right.start_time && output == right.output);
}

/**
 *  Compare two result.
 *
 *  @param[in] right The object to compare.
 *
 *  @return True if object have the different value.
 */
bool result::operator!=(result const& right) const noexcept {
  return !operator==(right);
}

/**************************************
 *                                     *
 *           Private Methods           *
 *                                     *
 **************************************/

/**
 *  Copy internal data members.
 *
 *  @param[in] right Object to copy.
 */
void result::_internal_copy(result const& right) {
  command_id = right.command_id;
  end_time = right.end_time;
  exit_code = right.exit_code;
  exit_status = right.exit_status;
  start_time = right.start_time;
  output = right.output;
}

namespace com::centreon::engine::commands {
std::ostream& operator<<(std::ostream& s, const result& to_dump) {
  s << "start_time=" << to_dump.start_time << ", end_time=" << to_dump.end_time
    << ", exit_code=" << to_dump.exit_code
    << ", exit_status=" << to_dump.exit_status << ", output='" << to_dump.output
    << '\'';
  return s;
}

}  // namespace com::centreon::engine::commands
