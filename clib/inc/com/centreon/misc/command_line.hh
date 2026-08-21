/**
 * Copyright 2011-2026 Centreon
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

#ifndef CC_MISC_COMMAND_LINE_HH
#define CC_MISC_COMMAND_LINE_HH

#include <string>
#include <string_view>
#include <vector>

namespace com::centreon::misc {

/**
 *  @class command_line command_line.hh "com/centreon/misc/command_line.hh"
 *  @brief Provide method to split command line arguments into array.
 *
 *  Command line is a simple way to split command line arguments
 *  into array.
 */
class command_line {
  /* The tokens, written one after the other and each terminated by a zero.
   *
   * @warning _argv points *into* this buffer, so anything reallocating it
   * invalidates the whole argument array — hence a single assign() up front in
   * parse(), and offsets recomputed in _rebase_argv() rather than the vector
   * being copied as is.
   *
   * That is also why no move operation is declared: the copy constructor and
   * the copy assignment suppress the implicit ones, and a defaulted move would
   * be wrong. For a command line of 15 bytes or less the small string
   * optimization keeps the characters inside the object itself, so moving the
   * string would leave _argv pointing into the moved-from object. std::move on
   * a command_line falls back to a copy, which is correct.
   */
  std::string _buffer;
  std::vector<char*> _argv;

  void _rebase_argv(const command_line& right);
  void _release();

 public:
  command_line() = default;
  explicit command_line(std::string_view cmdline);
  command_line(const command_line& right);
  ~command_line() noexcept = default;
  command_line& operator=(const command_line& right);
  int get_argc() const noexcept;
  char* const* get_argv() const noexcept;
  void parse(std::string_view cmdline);
};

}  // namespace com::centreon::misc

#endif  // !CC_MISC_COMMAND_LINE_HH
