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

#ifndef CCCS_CREDENTIALS_HH
#  define CCCS_CREDENTIALS_HH

#  include <string>
#  include "com/centreon/connector/ssh/namespace.hh"

CCCS_BEGIN()

/**
 *  @class credentials credentials.hh "com/centreon/connector/ssh/credentials.hh"
 *  @brief Connection credentials.
 *
 *  Bundle together connection credentials : host, user and
 *  password. Methods are provided so that they can be compared.
 */
class                credentials {
public:
                     credentials();
                     credentials(
                       std::string const& host,
                       std::string const& user,
                       std::string const& password);
                     credentials(credentials const& c);
                     ~credentials();
  credentials&       operator=(credentials const& c);
  bool               operator==(credentials const& c) const;
  bool               operator!=(credentials const& c) const;
  bool               operator<(credentials const& c) const;
  std::string const& get_host() const;
  std::string const& get_password() const;
  std::string const& get_user() const;
  void               set_host(std::string const& host);
  void               set_password(std::string const& password);
  void               set_user(std::string const& user);

private:
  void               _copy(credentials const& c);

  std::string        _host;
  std::string        _password;
  std::string        _user;
};

CCCS_END()

#endif // !CCCS_CREDENTIALS_HH
