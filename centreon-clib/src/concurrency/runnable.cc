/*
** Copyright 2011-2013 Centreon
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
**
** For more information : contact@centreon.com
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
