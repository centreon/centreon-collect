/**
 * Copyright 2011 Merethis
 * Copyright 2024 Centreon (https://www.centreon.com/)
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

#ifndef CCE_DIAGNOSTIC_HH
#define CCE_DIAGNOSTIC_HH

namespace com::centreon::engine {

/**
 *  @class diagnostic diagnostic.hh "com/centreon/engine/diagnostic.hh"
 *  @brief Diagnostic class.
 *
 *  Generate a diagnostic file that is useful for opening tickets
 *  against Merethis support center.
 */
class diagnostic {
 public:
  diagnostic();
  diagnostic(diagnostic const& right);
  ~diagnostic();
  diagnostic& operator=(diagnostic const& right);
  void generate(std::string const& cfg_file, std::string const& out_file = "");

 private:
  std::string _build_target_path(std::string const& base,
                                 std::string const& file);
  void _exec_and_write_to_file(std::string const& cmd,
                               std::string const& out_file);
  void _exec_cp(std::string const& src, std::string const& dst);
};

}  // namespace com::centreon::engine

#endif  // !CCE_DIAGNOSTIC_HH
