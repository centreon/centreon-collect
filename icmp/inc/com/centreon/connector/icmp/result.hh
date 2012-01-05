/*
** Copyright 2011 Merethis
**
** This file is part of Centreon Connector ICMP.
**
** Centreon Connector ICMP is free software: you can redistribute it
** and/or modify it under the terms of the GNU Affero General Public
** License as published by the Free Software Foundation, either version
** 3 of the License, or (at your option) any later version.
**
** Centreon Connector ICMP is distributed in the hope that it will be
** useful, but WITHOUT ANY WARRANTY; without even the implied warranty
** of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
** Affero General Public License for more details.
**
** You should have received a copy of the GNU Affero General Public
** License along with Centreon Connector ICMP. If not, see
** <http://www.gnu.org/licenses/>.
*/

#ifndef CCC_ICMP_RESULT_HH
#  define CCC_ICMP_RESULT_HH

#  include <sstream>
#  include <string>
#  include "com/centreon/connector/icmp/namespace.hh"

CCC_ICMP_BEGIN()

/**
 *  @class result result.hh "com/centreon/connector/icmp/result.hh"
 *  @brief Allow to create a result query to Centreon Engine.
 *
 *  This class allow to create a result query to Centreon Engine.
 */
class         result {
public:
  enum        type {
    none = 0,
    version = 1,
    execute = 3,
    quit = 5,
    error = 7
  };

              result(type id = none);
              result(result const& right);
  result&     operator=(result const& right);
  template<typename T>
  result&     operator<<(T const& argument) {
    std::ostringstream oss;
    oss << argument;
    _data += oss.str();
    _data.append(1, '\0');
    return (*this);
  }
              ~result() throw ();
  std::string data() const throw ();
  type        id() const throw ();

private:
  result&     _internal_copy(result const& right);

  std::string _data;
  type        _id;
};

CCC_ICMP_END()

#endif // !CCC_ICMP_RESULT_HH
