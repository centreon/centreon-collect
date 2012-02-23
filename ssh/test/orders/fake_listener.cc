/*
** Copyright 2011-2012 Merethis
**
** This file is part of Centreon Connector SSH.
**
** Centreon Connector SSH is free software: you can redistribute it
** and/or modify it under the terms of the GNU Affero General Public
** License as published by the Free Software Foundation, either version
** 3 of the License, or (at your option) any later version.
**
** Centreon Connector SSH is distributed in the hope that it will be
** useful, but WITHOUT ANY WARRANTY; without even the implied warranty
** of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
** Affero General Public License for more details.
**
** You should have received a copy of the GNU Affero General Public
** License along with Centreon Connector SSH. If not, see
** <http://www.gnu.org/licenses/>.
*/

#include <math.h>
#include "test/orders/fake_listener.hh"

using namespace com::centreon::connector::ssh::orders;

/**************************************
*                                     *
*           Public Methods            *
*                                     *
**************************************/

/**
 *  Default constructor.
 */
fake_listener::fake_listener() {}

/**
 *  Copy constructor.
 *
 *  @param[in] fl Object to copy.
 */
fake_listener::fake_listener(fake_listener const& fl) : listener(fl) {
  _copy(fl);
}

/**
 *  Destructor.
 */
fake_listener::~fake_listener() {}

/**
 *  Assignment operator.
 *
 *  @param[in] fl Object to copy.
 *
 *  @return This object.
 */
fake_listener& fake_listener::operator=(fake_listener const& fl) {
  if (this != &fl) {
    listener::operator=(fl);
    _copy(fl);
  }
  return (*this);
}

/**
 *  Get the callbacks information.
 *
 *  @return Callbacks information.
 */
std::list<fake_listener::callback_info> const& fake_listener::get_callbacks() const throw () {
  return (_callbacks);
}

/**
 *  EOF callback.
 */
void fake_listener::on_eof() {
  callback_info ci;
  ci.callback = cb_eof;
  _callbacks.push_back(ci);
  return ;
}

/**
 *  Error callback.
 */
void fake_listener::on_error() {
  callback_info ci;
  ci.callback = cb_error;
  _callbacks.push_back(ci);
  return ;
}

/**
 *  Execute callback.
 *
 *  @param[in] cmd_id   Command ID.
 *  @param[in] timeout  Timeout.
 *  @param[in] host     Host.
 *  @param[in] user     User.
 *  @param[in] password Password.
 *  @param[in] cmds     Commands.
 */
void fake_listener::on_execute(
                      unsigned long long cmd_id,
                      time_t timeout,
                      std::string const& host,
                      std::string const& user,
                      std::string const& password,
                      std::list<std::string> const& cmds) {
  callback_info ci;
  ci.callback = cb_execute;
  ci.cmd_id = cmd_id;
  ci.timeout = timeout;
  ci.host = host;
  ci.user = user;
  ci.password = password;
  ci.cmds = cmds;
  _callbacks.push_back(ci);
  return ;
}

/**
 *  Quit callback.
 */
void fake_listener::on_quit() {
  callback_info ci;
  ci.callback = cb_quit;
  _callbacks.push_back(ci);
  return ;
}

/**
 *  Version callback.
 */
void fake_listener::on_version() {
  callback_info ci;
  ci.callback = cb_version;
  _callbacks.push_back(ci);
  return ;
}

/**************************************
*                                     *
*           Private Methods           *
*                                     *
**************************************/

/**
 *  Copy internal data members.
 *
 *  @param[in] fl Object to copy.
 */
void fake_listener::_copy(fake_listener const& fl) {
  _callbacks = fl._callbacks;
  return ;
}

/**************************************
*                                     *
*           Global Objects.           *
*                                     *
**************************************/

/**
 *  Check callback_info list equality.
 *
 *  @param[in] left  First list.
 *  @param[in] right Second list.
 *
 *  @return true if both lists are equal.
 */
bool operator==(
       std::list<fake_listener::callback_info> const& left,
       std::list<fake_listener::callback_info> const& right) {
  bool retval(true);
  if (left.size() != right.size())
    retval = false;
  else {
    for (std::list<fake_listener::callback_info>::const_iterator
           it1 = left.begin(),
           end1 = left.end(),
           it2 = right.begin();
         it1 != end1;
         ++it1, ++it2)
      if ((it1->callback != it2->callback)
          || ((it1->callback == fake_listener::cb_execute)
              && ((it1->cmd_id != it2->cmd_id)
                  || (fabs(it1->timeout - it2->timeout) >= 1.0)
                  || (it1->host != it2->host)
                  || (it1->user != it2->user)
                  || (it1->password != it2->password)
                  || (it1->cmds != it2->cmds))))
        retval = false;
  }
  return (retval);
}

/**
 *  Check callback_info list inequality.
 *
 *  @param[in] left  First list.
 *  @param[in] right Second list.
 *
 *  @return true if both lists are inequal.
 */
bool operator!=(
       std::list<fake_listener::callback_info> const& left,
       std::list<fake_listener::callback_info> const& right) {
  return (!operator==(left, right));
}
