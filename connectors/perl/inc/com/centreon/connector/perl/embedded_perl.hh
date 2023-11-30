/*
** Copyright 2022 Centreon
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

#ifndef CCCP_EMBEDDED_PERL_HH
#define CCCP_EMBEDDED_PERL_HH

#include <EXTERN.h>
#include <perl.h>
#include <sys/types.h>

// Global Perl interpreter.
extern PerlInterpreter* my_perl;

namespace com::centreon::connector::perl {

/**
 *  @class embedded_perl embedded_perl.hh
 * "com/centreon/connector/perl/embedded_perl.hh"
 *  @brief Embedded Perl interpreter.
 *
 *  Embedded Perl interpreter wrapped in a singleton.
 */
class embedded_perl {
 public:
  ~embedded_perl();
  static embedded_perl& instance();
  static void load(int argc, char** argv, char** env, char const* code = NULL);
  pid_t run(std::string const& cmd,
            int fds[3],
            const shared_io_context& io_context);
  static void unload();

 private:
  using cmd_to_perl_map = absl::flat_hash_map<std::string, SV*>;

  embedded_perl(int argc, char** argv, char** env, char const* code = NULL);
  embedded_perl(embedded_perl const& ep);
  embedded_perl& operator=(embedded_perl const& ep);

  cmd_to_perl_map _parsed;
  static char const* const _script;
  pid_t _self;
  char** _argv;
};

}  // namespace com::centreon::connector::perl

#endif  // !CCCP_EMBEDDED_PERL_HH
