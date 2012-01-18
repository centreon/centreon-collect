/*
** Copyright 2011-2012 Merethis
**
** This file is part of Centreon Connector Perl.
**
** Centreon Connector Perl is free software: you can redistribute it
** and/or modify it under the terms of the GNU Affero General Public
** License as published by the Free Software Foundation, either version
** 3 of the License, or (at your option) any later version.
**
** Centreon Connector Perl is distributed in the hope that it will be
** useful, but WITHOUT ANY WARRANTY; without even the implied warranty
** of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
** Affero General Public License for more details.
**
** You should have received a copy of the GNU Affero General Public
** License along with Centreon Connector Perl. If not, see
** <http://www.gnu.org/licenses/>.
*/

#ifndef CCCP_EMBEDDED_PERL_HH
#  define CCCP_EMBEDDED_PERL_HH

#  include <string>
#  include <sys/types.h>
#  include <EXTERN.h>
#  include <perl.h>
#  include "com/centreon/connector/perl/namespace.hh"

// Global Perl interpreter.
extern PerlInterpreter*    my_perl;

CCCP_BEGIN()

/**
 *  @class embedded_perl embedded_perl.hh "com/centreon/connector/perl/embedded_perl.hh"
 *  @brief Embedded Perl interpreter.
 *
 *  Embedded Perl interpreter wrapped in a singleton.
 */
class                      embedded_perl {
public:
                           ~embedded_perl();
  static embedded_perl&    instance();
  static void              load(int* argc, char*** argv, char*** env);
  pid_t                    run(std::string const& cmd, int fds[3]);
  static void              unload();

private:
                           embedded_perl(
                             int* argc,
                             char*** argv,
                             char*** env);
                           embedded_perl(embedded_perl const& ep);
  embedded_perl&           operator=(embedded_perl const& ep);
  void                     _internal_copy(embedded_perl const& ep);

  static char const* const _script;
  pid_t                    _self;
};

CCCP_END()

#endif // !CCCP_EMBEDDED_PERL_HH
