/*
** Copyright 2011 Merethis
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

#ifndef CCC_PERL_MULTIPLEX_HH_
# define CCC_PERL_MULTIPLEX_HH_

# include <signal.h>
# include "com/centreon/connector/perl/namespace.hh"

CCC_PERL_BEGIN()

int     multiplex(sigset_t sigmask, bool with_stdin = true);

CCC_PERL_END()

#endif /* !CCC_PERL_MULTIPLEX_HH_ */
