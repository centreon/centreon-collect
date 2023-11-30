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

#ifndef CCCS_SESSIONS_CREDENTIALS_HH
#define CCCS_SESSIONS_CREDENTIALS_HH

#include <string>

namespace com::centreon::connector::ssh::sessions {

/**
 *  @class credentials credentials.hh
 * "com/centreon/connector/ssh/sessions/credentials.hh"
 *  @brief Connection credentials.
 *
 *  Bundle together connection credentials : host, user and
 *  password. Methods are provided so that they can be compared.
 */
class credentials {
 public:
  credentials();
  credentials(std::string const& host, std::string const& user,
              std::string const& password, std::string const& key = "",
              unsigned short port = 22);
  credentials(credentials const& c);
  ~credentials() = default;
  credentials& operator=(credentials const& c);
  bool operator==(credentials const& c) const;
  bool operator!=(credentials const& c) const;
  bool operator<(credentials const& c) const;
  std::string const& get_key() const;
  std::string const& get_host() const;
  std::string const& get_password() const;
  unsigned short get_port() const;
  std::string const& get_user() const;
  void set_host(std::string const& host);
  void set_key(std::string const& file);
  void set_password(std::string const& password);
  void set_port(unsigned short port);
  void set_user(std::string const& user);

 private:
  void _copy(credentials const& c);

  std::string _host;
  std::string _key;
  std::string _password;
  unsigned short _port;
  std::string _user;
};

std::ostream& operator<<(std::ostream&, const credentials&);

}  // namespace com::centreon::connector::ssh::sessions

namespace fmt {
// formatter specializations for fmt
template <>
struct formatter<com::centreon::connector::ssh::sessions::credentials>
    : ostream_formatter {};
}  // namespace fmt

#endif  // !CCCS_SESSIONS_CREDENTIALS_HH
