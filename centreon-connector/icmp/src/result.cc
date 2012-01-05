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

#include "com/centreon/connector/icmp/result.hh"

using namespace com::centreon::connector::icmp;

/**
 *  Default constructor.
 *
 *  @param[in] id  The result id.
 */
result::result(type id)
  : _id(id) {
  if (id != none)
    *this << id;
}

/**
 *  Default copy constructor.
 *
 *  @param[in] right  The object to copy.
 */
result::result(result const& right) {
  _internal_copy(right);
}

/**
 *  Default copy operator.
 *
 *  @param[in] right  The object to copy.
 *
 *  @return This object.
 */
result& result::operator=(result const& right) {
  return (_internal_copy(right));
}

/**
 *  Default destructor.
 */
result::~result() throw () {

}

/**
 *  Get the raw result data.
 *
 *  @return The raw data.
 */
std::string result::data() const throw () {
  return (std::string(_data).append(3, '\0'));
}

/**
 *  Get the result id.
 *
 *  @param[in] The id.
 */
result::type result::id() const throw () {
  return (_id);
}

/**
 *  Internal copy.
 *
 *  @param[in] right  The object to copy.
 *
 *  @return This object.
 */
result& result::_internal_copy(result const& right) {
  if (this != &right) {
    _data = right._data;
    _id = right._id;
  }
  return (*this);
}
