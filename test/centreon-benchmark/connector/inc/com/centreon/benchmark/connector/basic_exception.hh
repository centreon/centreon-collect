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

#ifndef CCB_CONNECTOR_BASIC_EXCEPTION
#  define CCB_CONNECTOR_BASIC_EXCEPTION

#  include <exception>
#  include "com/centreon/benchmark/connector/namespace.hh"

CCB_CONNECTOR_BEGIN()

/**
 *  @class basic_exception basic_exception.hh "com/centreon/benchmark/connector/basic_exception.hh"
 *  @brief Base exception class.
 *
 *  Simple exception class containing an basic error message.
 */
class              basic_exception : public std::exception {
public:
                   basic_exception(char const* message = "");
                   basic_exception(basic_exception const& right);
                   ~basic_exception() throw ();
  basic_exception& operator=(basic_exception const& right);
  char const*      what() const throw ();

private:
  basic_exception& _internal_copy(basic_exception const& right);

  char const*      _message;
};

CCB_CONNECTOR_END()

#endif // !CCB_CONNECTOR_BASIC_EXCEPTION
