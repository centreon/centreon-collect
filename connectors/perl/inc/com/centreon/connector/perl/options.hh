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

#ifndef CCCP_OPTIONS_HH
#define CCCP_OPTIONS_HH

#include "com/centreon/misc/get_options.hh"

CCCP_BEGIN()

/**
 *  @class options options.hh "com/centreon/connector/perl/options.hh"
 *  @brief Parse and expose command line arguments.
 *
 *  Parse and expose command line arguments.
 */
class options : public com::centreon::misc::get_options {
  void _init();

 public:
  options();
  ~options() noexcept override;
  options(options const& opts) = delete;
  options& operator=(options const& opts) = delete;
  std::string help() const override;
  void parse(int argc, char* argv[]);
  std::string usage() const override;
};

CCCP_END()

#endif  // !CCCP_OPTIONS_HH
