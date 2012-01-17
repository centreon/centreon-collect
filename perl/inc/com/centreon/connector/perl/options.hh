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

#ifndef CCCP_OPTIONS_HH
#  define CCCP_OPTIONS_HH

#  include "com/centreon/connector/perl/namespace.hh"
#  include "com/centreon/misc/get_options.hh"

CCCP_BEGIN()

/**
 *  @class options options.hh "com/centreon/connector/perl/options.hh"
 *  @brief Parse and expose command line arguments.
 *
 *  Parse and expose command line arguments.
 */
class         options : public com::centreon::misc::get_options {
public:
              options();
              options(options const& opts);
              ~options() throw ();
  options&    operator=(options const& opts);
  std::string help() const;
  void        parse(int argc, char* argv[]);
  std::string usage() const;

private:
  void        _init();
};

CCCP_END()

#endif // !CCCP_OPTIONS_HH
