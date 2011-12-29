/*
** Copyright 2011 Merethis
**
** This file is part of Centreon Connector SSH.
**
** Centreon Connector SSH is free software: you can redistribute it
** and/or modify it under the terms of the GNU Affero General Public
** License as published by the Free Software Foundation, either version
** 3 of the License, or (at your option) any later version.
**
** Centreon Connector SSH is distributed in the hope that it will be
** useful, but WITHOUT ANY WARRANTY; without even the implied warranty
** of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
** Affero General Public License for more details.
**
** You should have received a copy of the GNU Affero General Public
** License along with Centreon Connector SSH. If not, see
** <http://www.gnu.org/licenses/>.
*/

#ifndef CCCS_OPTIONS_HH
#  define CCCS_OPTIONS_HH

#  include "com/centreon/connector/ssh/namespace.hh"
#  include "com/centreon/misc/get_options.hh"

CCCS_BEGIN()

/**
 *  @class options options.hh "com/centreon/connector/ssh/options.hh"
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

CCCS_END()

#endif // !CCCS_OPTIONS_HH
