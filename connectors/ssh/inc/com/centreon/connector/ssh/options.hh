/**
 * Copyright 2011-2013 Centreon
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

#ifndef CCCS_OPTIONS_HH
#define CCCS_OPTIONS_HH

#include "com/centreon/misc/get_options.hh"

namespace com::centreon::connector::ssh {

/**
 *  @class options options.hh "com/centreon/connector/ssh/options.hh"
 *  @brief Parse and expose command line arguments.
 *
 *  Parse and expose command line arguments.
 */
class options : public com::centreon::misc::get_options {
 public:
  options();
  options(options const& opts);
  ~options() noexcept override;
  options& operator=(options const& opts);
  std::string help() const override;
  void parse(int argc, char* argv[]);
  std::string usage() const override;

 private:
  void _init();
};

}  // namespace com::centreon::connector::ssh

#endif  // !CCCS_OPTIONS_HH
