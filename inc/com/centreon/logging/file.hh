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

#ifndef CC_LOGGING_FILE_HH
#  define CC_LOGGING_FILE_HH

#  include <cstdio>
#  include <string>
#  include "com/centreon/concurrency/mutex.hh"
#  include "com/centreon/logging/backend.hh"
#  include "com/centreon/namespace.hh"

CC_BEGIN()

namespace              logging {
  /**
   *  @class file file.hh "com/centreon/logging/file.hh"
   *  @brief Log messages to file.
   */
  class                file : public backend {
  public:
                       file(FILE* file);
                       file(std::string const& path);
                       file(file const& right);
                       ~file() throw ();
    file&              operator=(file const& right);
    void               close() throw ();
    std::string const& filename() const throw ();
    void               flush() throw ();
    void               log(char const* msg, unsigned int size) throw ();
    void               open();
    void               reopen();

  private:
    file&              _internal_copy(file const& right);

    concurrency::mutex _mtx;
    std::string        _path;
    FILE*              _out;
  };
}

CC_END()

#endif // !CC_LOGGING_FILE_HH
