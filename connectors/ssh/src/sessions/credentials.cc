/**
* Copyright 2011-2013 Centreon
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
*     http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*
* For more information : contact@centreon.com
*/

#include "com/centreon/connector/ssh/sessions/credentials.hh"

using namespace com::centreon::connector::ssh::sessions;

/**************************************
 *                                     *
 *           Public Methods            *
 *                                     *
 **************************************/

/**
 *  @brief Default constructor.
 *
 *  Host, user, password and identity are all empty after construction.
 *  Port number are set to 22 by default.
 */
credentials::credentials() : _port(22) {}

/**
 *  Constructor.
 *
 *  @param[in] host     Host.
 *  @param[in] user     User.
 *  @param[in] password Password.
 *  @param[in] key      Identity file.
 *  @param[in] port     Port.
 */
credentials::credentials(std::string const& host,
                         std::string const& user,
                         std::string const& password,
                         std::string const& key,
                         unsigned short port)
    : _host(host), _key(key), _password(password), _port(port), _user(user) {}

/**
 *  Copy constructor.
 *
 *  @param[in] c Object to copy.
 */
credentials::credentials(credentials const& c) {
  _copy(c);
}

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
  return ((_port == c._port) && (_host == c._host) && (_key == c._key) &&
          (_password == c._password) && (_user == c._user));
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
  else if (_user != c._user)
    retval = (_user < c._user);
  else if (_password != c._password)
    retval = (_password < c._password);
  else if (_port != c._port)
    retval = (_port < c._port);
  else if (_key != c._key)
    retval = (_key < c._key);
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
 *  Get the key file.
 *
 *  @return Identity file.
 */
std::string const& credentials::get_key() const {
  return (_key);
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
 *  Get the port.
 *
 *  @return Port.
 */
unsigned short credentials::get_port() const {
  return (_port);
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
 *  Set key file.
 *
 *  @param[in] file The new identity file.
 */
void credentials::set_key(std::string const& file) {
  _key = file;
}

/**
 *  Set the host.
 *
 *  @param[in] host New host.
 */
void credentials::set_host(std::string const& host) {
  _host = host;
}

/**
 *  Set the password.
 *
 *  @param[in] password New password.
 */
void credentials::set_password(std::string const& password) {
  _password = password;
}

/**
 *  Set the port.
 *
 *  @param[in] port New port.
 */
void credentials::set_port(unsigned short port) {
  _port = port;
}

/**
 *  Set the user.
 *
 *  @param[in] user New user.
 */
void credentials::set_user(std::string const& user) {
  _user = user;
}

CCCS_BEGIN()

namespace sessions {
std::ostream& operator<<(std::ostream& s, const credentials& cred) {
  s << cred.get_user() << '@' << cred.get_host() << ':' << cred.get_port();
  return s;
}

}  // namespace sessions

CCCS_END()

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
  _key = c._key;
  _password = c._password;
  _port = c._port;
  _user = c._user;
}
