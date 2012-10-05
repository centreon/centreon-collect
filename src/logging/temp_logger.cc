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

#include "com/centreon/logging/temp_logger.hh"

using namespace com::centreon::logging;

/**
 *  Default constrcutor.
 *
 *  @param[in] type     Logging types.
 *  @param[in] verbose  Verbosity level.
 */
temp_logger::temp_logger(
               unsigned int type,
               verbosity verbose) throw ()
  : _engine(engine::instance()),
    _type(type),
    _verbose(verbose) {

}

/**
 *  Default copy constrcutor.
 *
 *  @param[in] right  The object to copy.
 */
temp_logger::temp_logger(temp_logger const& right)
  : _engine(right._engine) {
  _internal_copy(right);
}

/**
 *  Default destructor.
 */
temp_logger::~temp_logger() throw () {
  _engine.log(_type, _verbose, _buffer.data());
}

/**
 *  Default copy operator.
 *
 *  @param[in] right  The object to copy.
 *
 *  @return This object.
 */
temp_logger& temp_logger::operator=(temp_logger const& right) {
  return (_internal_copy(right));
}

/**
 *  Set float precision.
 *
 *  @param[in] obj The new precision.
 *
 *  @return This object.
 */
temp_logger& temp_logger::operator<<(setprecision const& obj) throw () {
  _buffer.precision(obj.precision);
  return (*this);
}

/**
 *  Internal copy.
 *
 *  @param[in] right  The object to copy.
 *
 *  @return This object.
 */
temp_logger& temp_logger::_internal_copy(temp_logger const& right) {
  if (this != &right) {
    _buffer = right._buffer;
    _type = right._type;
    _verbose = right._verbose;
  }
  return (*this);
}
