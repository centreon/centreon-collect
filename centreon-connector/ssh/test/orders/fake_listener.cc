/*
** Copyright 2011-2013 Merethis
**
** This file is part of Centreon SSH Connector.
**
** Centreon SSH Connector is free software: you can redistribute it
** and/or modify it under the terms of the GNU Affero General Public
** License as published by the Free Software Foundation, either version
** 3 of the License, or (at your option) any later version.
**
** Centreon SSH Connector is distributed in the hope that it will be
** useful, but WITHOUT ANY WARRANTY; without even the implied warranty
** of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
** Affero General Public License for more details.
**
** You should have received a copy of the GNU Affero General Public
** License along with Centreon SSH Connector. If not, see
** <http://www.gnu.org/licenses/>.
*/

#include <cmath>
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
 *
 *  @param[in] cmd_id Command ID.
 *  @param[in] msg    Error message.
 */
void fake_listener::on_error(
                      unsigned long long cmd_id,
                      char const* msg) {
  (void)cmd_id;
  (void)msg;
  callback_info ci;
  ci.callback = cb_error;
  _callbacks.push_back(ci);
  return ;
}

/**
 *  Execute callback.
 *
 *  @param[in] cmd_id      Command ID.
 *  @param[in] timeout     Timeout.
 *  @param[in] host        Host.
 *  @param[in] port        Connection port.
 *  @param[in] user        User.
 *  @param[in] password    Password.
 *  @param[in] identity    Identity file.
 *  @param[in] cmds        Commands.
 *  @param[in] skip_stdout Should stdout be skipped.
 *  @param[in] skip_stderr Should stderr be skipped.
 *  @param[in] is_ipv6     Work with IPv6.
 */
void fake_listener::on_execute(
                      unsigned long long cmd_id,
                      time_t timeout,
                      std::string const& host,
                      unsigned short port,
                      std::string const& user,
                      std::string const& password,
                      std::string const& identity,
                      std::list<std::string> const& cmds,
                      int skip_stdout,
                      int skip_stderr,
                      bool is_ipv6) {
  callback_info ci;
  ci.callback = cb_execute;
  ci.cmd_id = cmd_id;
  ci.timeout = timeout;
  ci.host = host;
  ci.port = port;
  ci.user = user;
  ci.password = password;
  ci.identity = identity;
  ci.cmds = cmds;
  ci.skip_stdout = skip_stdout;
  ci.skip_stderr = skip_stderr;
  ci.is_ipv6 = is_ipv6;
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
                  || (it1->port != it2->port)
                  || (it1->user != it2->user)
                  || (it1->password != it2->password)
                  || (it1->identity != it2->identity)
                  || (it1->skip_stdout != it2->skip_stdout)
                  || (it1->skip_stderr != it2->skip_stderr)
                  || (it1->is_ipv6 != it2->is_ipv6)
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
