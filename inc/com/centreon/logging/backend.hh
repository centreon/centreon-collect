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

#ifndef CC_LOGGING_BACKEND_HH
#  define CC_LOGGING_BACKEND_HH

#  include "com/centreon/namespace.hh"

CC_BEGIN()

namespace        logging {
  /**
   *  @class backend backend.hh "com/centreon/logging/backend.hh"
   *  @brief Base logging backend class.
   *
   *  This class defines an interface to create logger backend, to
   *  log data into many different objects.
   */
  class          backend {
  public:
                 backend();
    virtual      ~backend() throw ();
    virtual void flush() throw () = 0;
    virtual void log(char const* msg) throw ();
    virtual void log(char const* msg, unsigned int size) throw () = 0;
  };
}

CC_END()

#endif // !CC_LOGGING_BACKEND_HH
