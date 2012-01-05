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

#ifndef CCC_ICMP_REQUEST_HH
#  define CCC_ICMP_REQUEST_HH

#  include <sstream>
#  include <string>
#  include "com/centreon/connector/icmp/namespace.hh"

CCC_ICMP_BEGIN()

/**
 *  @class request request.hh "com/centreon/connector/icmp/request.hh"
 *  @brief Parse a raw data to get request argument.
 *
 *  This class alow to parse request raw data to get request arguments.
 */
class         request {
public:
  enum        type {
    version = 0,
    execute = 2,
    quit = 4
  };

              request(std::string const& data = "");
              request(request const& right);
              ~request() throw ();
  request&    operator=(request const& right);
  bool        empty() const throw ();
  type        id() const throw ();
  bool        next_argument(std::string& argument);
  template<typename T>
  bool        next_argument(T& argument) {
    std::string arg_str;
    if (!next_argument(arg_str))
      return (false);
    std::istringstream iss(arg_str);
    return ((iss >> argument) != 0);
  }

private:
  request&    _internal_copy(request const& right);

  std::string _data;
  type        _id;
};

CCC_ICMP_END()

#endif // !CCC_ICMP_REQUEST_HH
