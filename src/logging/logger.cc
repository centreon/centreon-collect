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

#include "com/centreon/logging/logger.hh"

using namespace com::centreon::logging;

com::centreon::logging::logger info(type_info, "info");
com::centreon::logging::logger debug(type_debug, "debug");
com::centreon::logging::logger error(type_error, "error");

logger::logger(type_number type, char const* prefix) throw ()
  : _type(type) {
  if (prefix)
    _prefix.append("[").append(prefix).append("] ");
}

/**
 *  Default copy constructor.
 *
 *  @param[in] right  The object to copy.
 *
 *  @return This object.
 */
logger::logger(logger const& right) {
  _internal_copy(right);
}

/**
 *  Default destructor.
 */
logger::~logger() throw () {

}

/**
 *  Default copy operator.
 *
 *  @param[in] right  The object to copy.
 *
 *  @return This object.
 */
logger& logger::operator=(logger const& right) {
  return (_internal_copy(right));
}

/**
 *  Parentheses operator.
 *
 *  @param[in] verbose  The verbosity level.
 *
 *  @return A new temp logger.
 */
temp_logger logger::operator()(verbosity const& verbose) const {
  return (temp_logger(_type, verbose) << _prefix);
}

/**
 *  Parentheses operator.
 *
 *  @param[in] level  The verbosity level.
 *
 *  @return A new temp logger.
 */
temp_logger logger::operator()(verbosity_level level) const {
  return (temp_logger(_type, verbosity(level)) << _prefix);
}

/**
 *  Internal copy.
 *
 *  @param[in] right  The object to copy.
 *
 *  @return This object.
 */
logger& logger::_internal_copy(logger const& right) {
  if (this != &right) {
    _prefix = right._prefix;
    _type = right._type;
  }
  return (*this);
}
