/*
** Copyright 2011 Merethis
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

#include "com/centreon/connector/ssh/credentials.hh"

using namespace com::centreon::connector::ssh;

/**************************************
*                                     *
*           Private Methods           *
*                                     *
**************************************/

/**
 *  Copy internal data members.
 *
 *  @param[in] c Object to copy.
 */
void credentials::_copy(credentials const& c) {
  _host = c._host;
  _password = c._password;
  _user = c._user;
  return ;
}

/**************************************
*                                     *
*           Public Methods            *
*                                     *
**************************************/

/**
 *  @brief Default constructor.
 *
 *  Host, user and password are all empty after construction.
 */
credentials::credentials() {}

/**
 *  Constructor.
 *
 *  @param[in] host     Host.
 *  @param[in] user     User.
 *  @param[in] password Password.
 */
credentials::credentials(std::string const& host,
                         std::string const& user,
                         std::string const& password)
  : _host(host), _password(password), _user(user) {}

/**
 *  Copy constructor.
 *
 *  @param[in] c Object to copy.
 */
credentials::credentials(credentials const& c) {
  _copy(c);
}

/**
 *  Destructor.
 */
credentials::~credentials() {}

/**
 *  Assignment operator.
 *
 *  @param[in] c Object to copy.
 *
 *  @return This object.
 */
credentials& credentials::operator=(credentials const& c) {
  if (&c != this)
    _copy(c);
  return (*this);
}

/**
 *  @brief Equality operator.
 *
 *  Check that this object is equal to another object.
 *
 *  @return true if both objects are equal.
 */
bool credentials::operator==(credentials const& c) const {
  return ((_host == c._host)
          && (_password == c._password)
          && (_user == c._user));
}

/**
 *  @brief Non-equality operator.
 *
 *  Check that this object is not equal to another object.
 *
 *  @return true if both objects are not equal.
 */
bool credentials::operator!=(credentials const& c) const {
  return (!this->operator==(c));
}

/**
 *  @brief Strictly-less-than operator.
 *
 *  Check that this object is strictly inferior than another object.
 *
 *  @return true if this object is strictly inferior to the other
 *          object.
 */
bool credentials::operator<(credentials const& c) const {
  bool retval;
  if (_host != c._host)
    retval = (_host < c._host);
  else if (_password != c._password)
    retval = (_password < c._password);
  else if (_user != c._user)
    retval = (_user < c._user);
  else
    retval = false;
  return (retval);
}

/**
 *  Get the host.
 *
 *  @return Host.
 */
std::string const& credentials::get_host() const {
  return (_host);
}

/**
 *  Get the password.
 *
 *  @return Password.
 */
std::string const& credentials::get_password() const {
  return (_password);
}

/**
 *  Get the user.
 *
 *  @return User.
 */
std::string const& credentials::get_user() const {
  return (_user);
}

/**
 *  Set the host.
 *
 *  @param[in] host New host.
 */
void credentials::set_host(std::string const& host) {
  _host = host;
  return ;
}

/**
 *  Set the password.
 *
 *  @param[in] password New password.
 */
void credentials::set_password(std::string const& password) {
  _password = password;
  return ;
}

/**
 *  Set the user.
 *
 *  @param[in] user New user.
 */
void credentials::set_user(std::string const& user) {
  _user = user;
  return ;
}
