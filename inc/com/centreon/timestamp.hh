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

#ifndef CC_TIMESTAMP_HH
#  define CC_TIMESTAMP_HH

#  include <time.h>
#  include "com/centreon/namespace.hh"

CC_BEGIN()

/**
 *  @class timestamp timestamp.hh "com/centreon/timestamp.hh"
 *  @brief Provide time management.
 *
 *  Allow to manage time easily.
 */
class              timestamp {
public:
                   timestamp(time_t second = 0, long usecond = 0);
                   timestamp(timestamp const& right);
                   ~timestamp() throw ();
  timestamp&       operator=(timestamp const& right);
  bool             operator==(timestamp const& right) const throw ();
  bool             operator!=(timestamp const& right) const throw ();
  bool             operator<(timestamp const& right) const throw ();
  bool             operator<=(timestamp const& right) const throw ();
  bool             operator>(timestamp const& right) const throw ();
  bool             operator>=(timestamp const& right) const throw ();
  timestamp        operator+(timestamp const& right) const;
  timestamp        operator-(timestamp const& right) const;
  timestamp&       operator+=(timestamp const& right);
  timestamp&       operator-=(timestamp const& right);
  void             add_msecond(long msecond);
  void             add_second(time_t second);
  void             add_usecond(long usecond);
  static timestamp now() throw ();
  void             sub_msecond(long msecond);
  void             sub_second(time_t second);
  void             sub_usecond(long usecond);
  long long        to_msecond() const throw ();
  time_t           to_second() const throw ();
  long long        to_usecond() const throw ();

private:
  timestamp&       _internal_copy(timestamp const& right);
  static void      _transfer(time_t* second, long* usecond);

  long             _usecond;
  time_t           _second;
};

CC_END()

#endif // !CC_TIMESTAMP_HH
