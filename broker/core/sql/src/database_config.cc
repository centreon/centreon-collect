/**
 * Copyright 2014-2024 Centreon
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

#include "com/centreon/broker/sql/database_config.hh"
#include <absl/strings/ascii.h>
#include <boost/beast.hpp>
#include "com/centreon/broker/config/endpoint.hh"
#include "com/centreon/broker/exceptions/config.hh"
#include "com/centreon/common/http/http_config.hh"
#include "com/centreon/common/http/https_connection.hh"
#include "com/centreon/common/pool.hh"
#include "common/log_v2/log_v2.hh"
#include "common/vault/vault_access.hh"

using namespace com::centreon::broker;
using com::centreon::common::log_v2::log_v2;
using namespace com::centreon::common::http;

namespace com::centreon::broker {
std::ostream& operator<<(std::ostream& s, const database_config cfg) {
  s << cfg.get_type() << ": " << cfg.get_user() << '@';
  if (cfg.get_socket().empty()) {
    s << cfg.get_host() << ':' << cfg.get_port();
  } else {
    s << cfg.get_socket();
  }
  s << " queries per transaction:" << cfg.get_queries_per_transaction()
    << " check replication:" << cfg.get_check_replication()
    << " connection count:" << cfg.get_connections_count()
    << " max commit delay:" << cfg.get_max_commit_delay() << 's'
    << " extension_directory" << cfg.get_extension_directory();
  return s;
}

}  // namespace com::centreon::broker

/**
 *  Default constructor.
 */
database_config::database_config()
    : _queries_per_transaction(1),
      _check_replication(true),
      _connections_count(1),
      _category(SHARED),
      _extension_directory(DEFAULT_MARIADB_EXTENSION_DIR),
      _config_logger{log_v2::instance().get(log_v2::CONFIG)} {}

/**
 *  Constructor.
 *
 *  @param[in] type                     DB type ("mysql", "oracle",
 *                                      ...).
 *  @param[in] host                     The host machine.
 *  @param[in] port                     Connection port.
 *  @param[in] user                     The user login.
 *  @param[in] password                 The password.
 *  @param[in] name                     Database name.
 *  @param[in] queries_per_transaction  Number of queries allowed within
 *                                      a transaction before a commit
 *                                      occurs.
 *  @param[in] check_replication        Whether or not the replication
 *                                      status of the database should be
 *                                      checked.
 */
database_config::database_config(const std::string& type,
                                 const std::string& host,
                                 const std::string& socket,
                                 uint16_t port,
                                 const std::string& user,
                                 const std::string& password,
                                 const std::string& name,
                                 uint32_t queries_per_transaction,
                                 bool check_replication,
                                 int32_t connections_count,
                                 unsigned max_commit_delay)
    : _type(type),
      _host(host),
      _socket(socket),
      _port(port),
      _user(user),
      _password(password),
      _name(name),
      _queries_per_transaction(queries_per_transaction),
      _check_replication(check_replication),
      _connections_count(connections_count),
      _max_commit_delay(max_commit_delay),
      _category(SHARED),
      _extension_directory(DEFAULT_MARIADB_EXTENSION_DIR),
      _config_logger{log_v2::instance().get(log_v2::CONFIG)} {}

/**
 *  Build a database configuration from a configuration set.
 *
 *  @param[in] cfg  Endpoint configuration.
 */
database_config::database_config(
    const config::endpoint& cfg,
    const std::map<std::string, std::string>& global_params)
    : _extension_directory{DEFAULT_MARIADB_EXTENSION_DIR},
      _config_logger{log_v2::instance().get(log_v2::CONFIG)} {
  std::string env_file;
  {
    auto found = global_params.find("env_file");
    if (found != global_params.end()) {
      env_file = found->second;
      _config_logger->debug("Env file '{}' used.", env_file);
    } else {
      env_file = "/usr/share/centreon/.env";
      _config_logger->debug(
          "No env_file provided in Broker configuration, default one used.");
    }
  }
  std::string vault_file;
  {
    auto found = global_params.find("vault_configuration");
    if (found != global_params.end()) {
      vault_file = found->second;
      _config_logger->debug("Vault configuration file '{}' used.", vault_file);
    } else {
      _config_logger->debug(
          "No vault configuration file provided in Broker configuration.");
    }
  }
  bool verify_peer = true;
  {
    auto found = global_params.find("verify_vault_peer");
    if (found != global_params.end()) {
      if (absl::SimpleAtob(found->second, &verify_peer)) {
        _config_logger->debug("Verify Vault peer {}.",
                              verify_peer ? "enabled" : "disabled");
      } else {
        _config_logger->debug("Verification of Vault peer enabled by default.");
        verify_peer = true;
      }
    } else {
      _config_logger->debug("Verification of Vault peer enabled by default.");
    }
  }

  // db_type
  auto found = cfg.params.find("db_type");
  if (found != cfg.params.end())
    _type = found->second;
  else
    throw exceptions::config("no 'db_type' defined for endpoint '{}'",
                             cfg.name);

  // db_host
  found = cfg.params.find("db_host");
  if (found != cfg.params.end())
    _host = found->second;
  else
    _host = "localhost";

  // db_socket
  if (_host == "localhost") {
    found = cfg.params.find("db_socket");
    if (found != cfg.params.end())
      _socket = found->second;
    else
      _socket = MYSQL_SOCKET;
  } else
    _socket = "";

  // db_port
  found = cfg.params.find("db_port");
  if (found != cfg.params.end()) {
    uint32_t port;
    if (!absl::SimpleAtoi(found->second, &port)) {
      _config_logger->error(
          "In the database configuration, 'db_port' should be a number, "
          "and not '{}'",
          found->second);
      _port = 0;
    } else
      _port = port;
  } else
    _port = 0;

  // db_user
  found = cfg.params.find("db_user");
  if (found != cfg.params.end())
    _user = found->second;

  // db_password
  found = cfg.params.find("db_password");
  if (found != cfg.params.end())
    _password = found->second;

  try {
    common::vault::vault_access vault(env_file, vault_file, verify_peer,
                                      _config_logger);
    _password = vault.decrypt(_password);
    _config_logger->info("Database password get from Vault configuration");
  } catch (const std::exception& e) {
    constexpr std::string_view password_prefix("secret::hashicorp_vault::");
    std::string_view password_header(_password.data(), password_prefix.size());
    if (password_header == password_prefix)
      _config_logger->error("No usable Vault configuration: {}", e.what());
  }

  // db_name
  found = cfg.params.find("db_name");
  if (found != cfg.params.end())
    _name = found->second;
  else
    throw exceptions::config("no 'db_name' defined for endpoint '{}'",
                             cfg.name);

  // queries_per_transaction
  found = cfg.params.find("queries_per_transaction");
  if (found != cfg.params.end()) {
    if (!absl::SimpleAtoi(found->second, &_queries_per_transaction)) {
      _config_logger->error(
          "queries_per_transaction is a number but must be given as a "
          "string. "
          "Unable to read the value '{}' - value 2000 taken by default.",
          found->second);
      _queries_per_transaction = 2000;
    }
  } else
    _queries_per_transaction = 2000;

  // check_replication
  found = cfg.params.find("check_replication");
  if (found != cfg.params.end()) {
    if (!absl::SimpleAtob(found->second, &_check_replication)) {
      _config_logger->error(
          "check_replication is a string containing a boolean. If not "
          "specified, it will be considered as \"true\".");
      _check_replication = true;
    }
  } else
    _check_replication = true;

  // connections_count
  found = cfg.params.find("connections_count");
  if (found != cfg.params.end()) {
    if (!absl::SimpleAtoi(found->second, &_connections_count)) {
      _config_logger->error(
          "connections_count is a string "
          "containing an integer. If not "
          "specified, it will be considered as "
          "\"1\".");
      _connections_count = 1;
    }
  } else
    _connections_count = 1;
  found = cfg.params.find("max_commit_delay");
  if (found != cfg.params.end()) {
    if (!absl::SimpleAtoi(found->second, &_max_commit_delay)) {
      _config_logger->error(
          "max_commit_delay is a string "
          "containing an integer. If not "
          "specified, it will be considered as "
          "\"5\".");
      _max_commit_delay = 5;
    }
  } else
    _max_commit_delay = 5;

  found = cfg.params.find("extension_directory");
  if (found != cfg.params.end()) {
    _extension_directory = found->second;
  }
}

/**
 *  Copy constructor.
 *
 *  @param[in] other  Object to copy.
 */
database_config::database_config(const database_config& other) {
  _internal_copy(other);
}

/**
 *  Assignment operator.
 *
 *  @param[in] other  Object to copy.
 *
 *  @return This object.
 */
database_config& database_config::operator=(const database_config& other) {
  if (this != &other)
    _internal_copy(other);
  return *this;
}

/**
 *  Comparaison operator.
 *
 *  @param[in] other  Object to compared.
 *
 *  @return true if equal.
 */
bool database_config::operator==(database_config const& other) const {
  if (this != &other) {
    bool retval{_type == other._type && _host == other._host &&
                _socket == other._socket && _port == other._port &&
                _user == other._user && _password == other._password &&
                _name == other._name &&
                _queries_per_transaction == other._queries_per_transaction &&
                _connections_count == other._connections_count &&
                _max_commit_delay == other._max_commit_delay};
    if (!retval) {
      auto logger = log_v2::instance().get(log_v2::SQL);
      if (_type != other._type)
        logger->debug(
            "database configurations do not match because of their types: {} "
            "!= {}",
            _type, other._type);
      else if (_host != other._host)
        logger->debug(
            "database configurations do not match because of their hosts: {} "
            "!= {}",
            _host, other._host);
      else if (_socket != other._socket)
        logger->debug(
            "database configurations do not match because of their sockets: "
            "{} != {}",
            _socket, other._socket);
      else if (_port != other._port)
        logger->debug(
            "database configurations do not match because of their ports: {} "
            "!= {}",
            _port, other._port);
      else if (_user != other._user)
        logger->debug(
            "database configurations do not match because of their users: {} "
            "!= {}",
            _user, other._user);
      else if (_password != other._password)
        logger->debug(
            "database configurations do not match because of their "
            "passwords: {} != {}",
            _password, other._password);
      else if (_name != other._name)
        logger->debug(
            "database configurations do not match because of their names: {} "
            "!= {}",
            _name, other._name);
      else if (_queries_per_transaction != other._queries_per_transaction)
        logger->debug(
            "database configurations do not match because of their queries "
            "per transactions: {} != {}",
            _queries_per_transaction, other._queries_per_transaction);
      else if (_connections_count != other._connections_count)
        logger->debug(
            "database configurations do not match because of their "
            "connections counts: {} != {}",
            _connections_count, other._connections_count);
      else if (_max_commit_delay != other._max_commit_delay)
        logger->debug(
            "database configurations do not match because of their commit "
            "delay: {} != {}",
            _max_commit_delay, other._max_commit_delay);
      return false;
    }
  }
  return true;
}

/**
 *  Comparaison operator.
 *
 *  @param[in] other  Object to compared.
 *
 *  @return true if equal.
 */
bool database_config::operator!=(database_config const& other) const {
  return !operator==(other);
}

/**
 *  Get DB type.
 *
 *  @return The DB type.
 */
std::string const& database_config::get_type() const {
  return _type;
}

/**
 *  Get the DB host.
 *
 *  @return The DB host
 */
std::string const& database_config::get_host() const {
  return _host;
}

/**
 *  Get the DB socket.
 *
 *  @return The DB socket
 */
std::string const& database_config::get_socket() const {
  return _socket;
}

/**
 *  Get the connection port.
 *
 *  @return The connection port.
 */
unsigned short database_config::get_port() const {
  return _port;
}

/**
 *  Get user.
 *
 *  @return The user.
 */
std::string const& database_config::get_user() const {
  return _user;
}

/**
 *  Get password.
 *
 *  @return The password.
 */
std::string const& database_config::get_password() const {
  return _password;
}

/**
 *  Get DB name.
 *
 *  @return The database name.
 */
std::string const& database_config::get_name() const {
  return _name;
}

/**
 *  Get the number of queries per transaction.
 *
 *  @return Number of queries per transaction.
 */
uint32_t database_config::get_queries_per_transaction() const {
  return _queries_per_transaction;
}

/**
 *  Check whether or not database replication should be checked.
 *
 *  @return Database replication check flag.
 */
bool database_config::get_check_replication() const {
  return _check_replication;
}

/**
 *  Get the number of connections to open to the database server.
 *
 *  @return Number of connections.
 */
int database_config::get_connections_count() const {
  return _connections_count;
}

/**
 * @brief get max commit delay
 *
 * @return unsigned delay in seconds
 */
unsigned database_config::get_max_commit_delay() const {
  return _max_commit_delay;
}

/**
 * @brief get_category
 * mysql_manager try to factorise mysql connections and share the same
 * connection between clients that have the same configuration
 * (mysql_connection::match_config method)
 * category must be used if you don't want to use this sharing
 * only client that have the same config host, port, user, password, socket,
 * queries_per_transaction and category can share the same connection
 *
 * @return unsigned _category field
 */
unsigned database_config::get_category() const {
  return _category;
}

/**
 *  Set type.
 *
 *  @param[in] type  The database type.
 */
void database_config::set_type(std::string const& type) {
  _type = type;
}

/**
 *  Set host.
 *
 *  @param[in] host  The host.
 */
void database_config::set_host(std::string const& host) {
  _host = host;
}

/**
 *  Set socket.
 *
 *  @param[in] socket  The socket.
 */
void database_config::set_socket(std::string const& socket) {
  _socket = socket;
}

/**
 *  Set port.
 *
 *  @param[in] port  Set the port number of the database server.
 */
void database_config::set_port(unsigned short port) {
  _port = port;
}

/**
 *  Set user.
 *
 *  @param[in] user  The user name.
 */
void database_config::set_user(std::string const& user) {
  _user = user;
}

/**
 *  Set password.
 *
 *  @param[in] password  The password.
 */
void database_config::set_password(std::string const& password) {
  _password = password;
}

/**
 *  Set the database name.
 *
 *  @param[in] name  The database name.
 */
void database_config::set_name(std::string const& name) {
  _name = name;
}

/**
 *  Set the number of queries per transaction.
 *
 *  @param[in] qpt  Number of queries per transaction.
 */
void database_config::set_queries_per_transaction(int qpt) {
  _queries_per_transaction = qpt;
}

/**
 *  Set the number of connections.
 *
 *  @param[in] qpt  Number of connections.
 */
void database_config::set_connections_count(int count) {
  _connections_count = count;
}

/**
 *  Set whether or not database replication should be checked.
 *
 *  @param[in] check_replication  Replication check flag.
 */
void database_config::set_check_replication(bool check_replication) {
  _check_replication = check_replication;
}

/**
 *  Set the category of the connections (connections in mysql_manager aren't
 * shared if cfg have different category).
 *
 *  @param[in] category  Replication check flag.
 */
void database_config::set_category(unsigned category) {
  _category = category;
}

/**
 *  Copy internal data members.
 *
 *  @param[in] other  Object to copy.
 */
void database_config::_internal_copy(database_config const& other) {
  _type = other._type;
  _host = other._host;
  _socket = other._socket;
  _port = other._port;
  _user = other._user;
  _password = other._password;
  _name = other._name;
  _queries_per_transaction = other._queries_per_transaction;
  _check_replication = other._check_replication;
  _connections_count = other._connections_count;
  _max_commit_delay = other._max_commit_delay;
  _extension_directory = other._extension_directory;
}

/**
 * @brief create a copy of the object excepts that queries_per_transaction is
 * set to 0
 *
 * @param cfg
 * @return database_config
 */
database_config database_config::auto_commit_conf() const {
  database_config ret(*this);
  ret.set_queries_per_transaction(0);
  return ret;
}
