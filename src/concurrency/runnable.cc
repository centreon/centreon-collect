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

#include "com/centreon/concurrency/runnable.hh"

using namespace com::centreon::concurrency;

/**
 *  Default constructor.
 */
runnable::runnable()
  : _auto_delete(false) {

}

/**
 *  Default copy constructor.
 *
 *  @param[in] right The Object to copy.
 */
runnable::runnable(runnable const& right) {
  _internal_copy(right);
}

/**
 *  Default destructor.
 */
runnable::~runnable() throw () {

}

/**
 *  Default copy operator.
 *
 *  @param[in] right The Object to copy.
 *
 *  @return This object.
 */
runnable& runnable::operator=(runnable const& right) {
  return (_internal_copy(right));
}

/**
 *  Get the auto delete value.
 *
 *  @return True if this runnable need to be delete,
 *  otherwise false.
 */
bool runnable::get_auto_delete() const throw () {
  return (_auto_delete);
}

/**
 *  Set the auto delete value
 *
 *  @param[in] auto_delete  Set if this runnable need to be delete.
 */
void runnable::set_auto_delete(bool auto_delete) throw () {
  _auto_delete = auto_delete;
}

/**
 *  Internal copy.
 *
 *  @param[in] right The Object to copy.
 *
 *  @return This object.
 */
runnable& runnable::_internal_copy(runnable const& right) {
  if (this != &right) {
    _auto_delete = right._auto_delete;
  }
  return (*this);
}
