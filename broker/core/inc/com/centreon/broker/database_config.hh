/*
** Copyright 2014-2015 Centreon
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

#ifndef CCB_DATABASE_CONFIG_HH
#define CCB_DATABASE_CONFIG_HH

#include "com/centreon/broker/namespace.hh"

CCB_BEGIN()

// Forward declaration.
namespace config {
class endpoint;
}

/**
 *  @class database_config database_config.hh
 * "com/centreon/broker/database_config.hh"
 *  @brief Database configuration.
 *
 *  Hold the database information.
 */
class database_config {
 public:
  database_config();
  database_config(std::string const& type,
                  std::string const& host,
                  std::string const& socket,
                  unsigned short port,
                  std::string const& user,
                  std::string const& password,
                  std::string const& name,
                  uint32_t queries_per_transaction = 1,
                  bool check_replication = true,
                  int connections_count = 1,
                  unsigned max_commit_delay = 5);
  database_config(config::endpoint const& cfg);
  database_config(database_config const& other);
  ~database_config();
  database_config& operator=(database_config const& other);
  bool operator==(database_config const& other) const;
  bool operator!=(const database_config& other) const;

  std::string const& get_type() const;
  std::string const& get_host() const;
  std::string const& get_socket() const;
  unsigned short get_port() const;
  std::string const& get_user() const;
  std::string const& get_password() const;
  std::string const& get_name() const;
  uint32_t get_queries_per_transaction() const;
  bool get_check_replication() const;
  int get_connections_count() const;
  unsigned get_max_commit_delay() const;

  void set_type(std::string const& type);
  void set_host(std::string const& host);
  void set_socket(std::string const& socket);
  void set_port(unsigned short port);
  void set_user(std::string const& user);
  void set_password(std::string const& password);
  void set_name(std::string const& name);
  void set_connections_count(int count);
  void set_queries_per_transaction(int qpt);
  void set_check_replication(bool check_replication);

 private:
  void _internal_copy(database_config const& other);

  std::string _type;
  std::string _host;
  std::string _socket;
  unsigned short _port;
  std::string _user;
  std::string _password;
  std::string _name;
  int _queries_per_transaction;
  bool _check_replication;
  int _connections_count;
  unsigned _max_commit_delay;
};

std::ostream& operator<<(std::ostream& s, const database_config cfg);

CCB_END()

namespace fmt {
// formatter specializations for fmt
template <>
struct formatter<com::centreon::broker::database_config> : ostream_formatter {};

}  // namespace fmt

#endif  // !CCB_DATABASE_CONFIG_HH
