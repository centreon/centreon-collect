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

#include "com/centreon/exceptions/basic.hh"
#include "com/centreon/connector/icmp/request.hh"

using namespace com::centreon::connector::icmp;

/**
 *  Default constructor.
 *
 *  @param[in] data  This is a raw request.
 */
request::request(std::string const& data)
  : _data(data) {
  unsigned int id(0);
  if (!next_argument(id)
      || (id != request::version
          && id != request::execute
          && id != request::quit))
    throw (basic_error() << "invalid request:id not found");
  _id = static_cast<request::type>(id);
}

/**
 *  Default copy constructor.
 *
 *  @param[in] right  The object to copy.
 */
request::request(request const& right) {
  _internal_copy(right);
}

/**
 *  Default destructor.
 */
request::~request() throw () {

}

/**
 *  Default copy operator.
 *
 *  @param[in] right  The object to copy.
 *
 *  @return This object.
 */
request& request::operator=(request const& right) {
  return (_internal_copy(right));
}

/**
 *  Get if has a next argument or if is empty.
 *
 *  @return True if empty, false if has an argument.
 */
bool request::empty() const throw () {
  return (_data.empty());
}

/**
 *  Get the request id.
 *
 *  @return The id.
 */
request::type request::id() const throw () {
  return (_id);
}

/**
 *  Get the next request argument.
 *
 *  @param[out] argument  The argument.
 *
 *  @return True if an argument is available, otherwise false.
 */
bool request::next_argument(std::string& argument) {
  if (_data.empty())
    return (false);
  size_t pos(_data.find('\0', 0));
  argument = _data.substr(0, pos);
  _data.erase(0, pos == std::string::npos ? pos : pos + 1);
  return (true);
}

/**
 *  Internal copy.
 *
 *  @param[in] right  The object to copy.
 *
 *  @return This object.
 */
request& request::_internal_copy(request const& right) {
  if (this != &right) {
    _data = right._data;
    _id = right._id;
  }
  return (*this);
}
