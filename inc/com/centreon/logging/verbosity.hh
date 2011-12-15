/*
** Copyright 2011 Merethis
**
** This file is part of Centreon Clib.
**
** Centreon Clib is free software: you can redistribute it
** and/or modify it under the terms of the GNU Affero General Public
** License as published by the Free Software Foundation, either version
** 3 of the License, or (at your option) any later version.
**
** Centreon Clib is distributed in the hope that it will be
** useful, but WITHOUT ANY WARRANTY; without even the implied warranty
** of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
** Affero General Public License for more details.
**
** You should have received a copy of the GNU Affero General Public
** License along with Centreon Clib. If not, see
** <http://www.gnu.org/licenses/>.
*/

#ifndef CC_LOGGING_VERBOSITY_HH
#  define CC_LOGGING_VERBOSITY_HH

#  include <limits.h>
#  include "com/centreon/logging/verbosity.hh"
#  include "com/centreon/namespace.hh"

CC_BEGIN()

namespace        logging {
  /**
   *  @class verbosity verbosity.hh "com/centreon/logging/verbosity.hh"
   *  @brief Define the level priority of log messages.
   */
  class          verbosity {
  public:
                 verbosity(unsigned int val = 0);
                 verbosity(verbosity const& right);
                 ~verbosity() throw ();
    verbosity&   operator=(verbosity const& right);
    bool         operator==(verbosity const& right) const throw ();
    bool         operator!=(verbosity const& right) const throw ();
    bool         operator<(verbosity const& right) const throw ();
    bool         operator<=(verbosity const& right) const throw ();
    bool         operator>(verbosity const& right) const throw ();
    bool         operator>=(verbosity const& right) const throw ();
                 operator unsigned int();

  private:
    verbosity&   _internal_copy(verbosity const& right);

    unsigned int _val:6;
  };
}

CC_END()

#endif // !CC_LOGGING_VERBOSITY_HH
