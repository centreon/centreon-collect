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

#ifndef CC_STANDARD_OUTPUT_HH
#  define CC_STANDARD_OUTPUT_HH

#  include "com/centreon/handle.hh"
#  include "com/centreon/namespace.hh"

CC_BEGIN()

namespace            io {
  /**
   *  @class standard_output standard_output.hh "com/centreon/standard_output.hh"
   *  @brief Implementation of standrad output.
   *
   *  This class is an implementation of handle to use standard output.
   */
  class              standard_output : public handle {
  public:
                     standard_output();
                     standard_output(standard_output const& right);
                     ~standard_output() throw ();
    standard_output& operator=(standard_output const& right);
    void             close();
    unsigned long    read(void* data, unsigned long size);
    unsigned long    write(void const* data, unsigned long size);

  private:
    standard_output& _internal_copy(standard_output const& right);
  };
}

CC_END()

#endif // !CC_STANDARD_OUTPUT_HH
