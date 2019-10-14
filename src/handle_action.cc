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

#include "com/centreon/handle.hh"
#include "com/centreon/handle_action.hh"
#include "com/centreon/handle_listener.hh"

using namespace com::centreon;

/**************************************
*                                     *
*           Public Methods            *
*                                     *
**************************************/

/**
 *  Constructor.
 *
 *  @param[in] h             Handle.
 *  @param[in] hl            Listener
 *  @param[in] is_threadable Can this task be threaded ?
 */
handle_action::handle_action(handle* h, handle_listener* hl, bool is_threadable)
    : _action(none), _h(h), _hl(hl), _is_threadable(is_threadable) {}

/**
 *  Copy constructor.
 *
 *  @param[in] right Object to copy.
 */
handle_action::handle_action(handle_action const& right) : task(right) {
  _internal_copy(right);
}

/**
 *  Destructor.
 */
handle_action::~handle_action() throw() {}

/**
 *  Assignment operator.
 *
 *  @param[in] right Object to copy.
 *
 *  @return This object.
 */
handle_action& handle_action::operator=(handle_action const& right) {
  if (this != &right) {
    task::operator=(right);
    _internal_copy(right);
  }
  return (*this);
}

/**
 *  Is this task threadable ?
 *
 *  @return true if the task is threadable.
 */
bool handle_action::is_threadable() const throw() { return (_is_threadable); }

/**
 *  Get the handle.
 *
 *  @return Handle.
 */
handle* handle_action::get_handle() const throw() { return (_h); }

/**
 *  Get the listener.
 *
 *  @return Listener.
 */
handle_listener* handle_action::get_handle_listener() const throw() {
  return (_hl);
}

/**
 *  Run the task.
 */
void handle_action::run() {
  action a(_action);
  _action = none;
  if (a == error)
    _hl->error(*_h);
  else if (a == read)
    _hl->read(*_h);
  else if (a == write)
    _hl->write(*_h);
  return;
}

/**
 *  Set action.
 *
 *  @param[in] a Action to perform.
 */
void handle_action::set_action(action a) throw() {
  _action = a;
  return;
}

/**************************************
*                                     *
*           Private Methods           *
*                                     *
**************************************/

/**
 *  Copy internal data members.
 *
 *  @param[in] right Object to copy.
 */
void handle_action::_internal_copy(handle_action const& right) {
  _action = right._action;
  _h = right._h;
  _hl = right._hl;
  _is_threadable = right._is_threadable;
  return;
}
