/**
 * Copyright 2013, 2024 Centreon
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

#ifndef CCB_MISC_DIAGNOSTIC_HH
#define CCB_MISC_DIAGNOSTIC_HH

namespace com::centreon::broker {

namespace misc {
/**
 *  @class diagnostic diagnostic.hh "com/centreon/broker/misc/diagnostic.hh"
 *  @brief Generate diagnostic files.
 *
 *  Generate diagnostic files to resolve Centreon Broker issues.
 */
class diagnostic {
  std::shared_ptr<spdlog::logger> _logger;

 public:
  diagnostic();
  diagnostic(diagnostic const&) = delete;
  ~diagnostic() noexcept = default;
  diagnostic& operator=(diagnostic const&) = delete;
  void generate(std::vector<std::string> const& cfg_files,
                std::string const& out_file = "");
  static int exec_process(char const** argv, bool wait_for_completion);
};
}  // namespace misc

}  // namespace com::centreon::broker

#endif  // !CCB_MISC_DIAGNOSTIC_HH
