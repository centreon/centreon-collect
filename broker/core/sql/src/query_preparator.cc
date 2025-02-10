/**
 * Copyright 2015, 2020-2022 Centreon
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

#include "com/centreon/broker/sql/query_preparator.hh"

#include "com/centreon/broker/io/events.hh"
#include "com/centreon/broker/mapping/entry.hh"
#include "com/centreon/broker/sql/mysql.hh"
#include "com/centreon/exceptions/msg_fmt.hh"
#include "common/log_v2/log_v2.hh"

using namespace com::centreon::exceptions;
using namespace com::centreon::broker;
using namespace com::centreon::broker::database;
using com::centreon::common::log_v2::log_v2;

/**
 *  Constructor.
 *
 *  @param[in] event_id  Event ID.
 *  @param[in] unique    Event UNIQUE (for legacy events).
 *  @param[in] excluded  Fields excluded from the query.
 */
query_preparator::query_preparator(
    uint32_t event_id,
    query_preparator::event_unique const& unique,
    query_preparator::excluded_fields const& excluded)
    : _logger{log_v2::instance().get(log_v2::SQL)},
      _event_id(event_id),
      _excluded(excluded),
      _unique(unique) {}

/**
 *  Constructor.
 *
 *  @param[in] event_id  Event ID.
 *  @param[in] pb_unique    Event UNIQUE (for protobuf events).
 */
query_preparator::query_preparator(
    uint32_t event_id,
    const query_preparator::event_pb_unique& pb_unique)
    : _logger{log_v2::instance().get(log_v2::SQL)},
      _event_id(event_id),
      _pb_unique(pb_unique) {}

/**
 * @brief Prepare an INSERT query. The function waits for the table to insert
 * into, and a mapping between indexes and column names. Indexes come from the
 * protobuf declaration of the object.
 *
 * @param ms  the mysql object, owner of the prepared statements.
 * @param table The table concerned by the INSERT.
 * @param mapping The correspondance between indexes and column names.
 * @param ignore A boolean telling if error should be ignored.
 *
 * @return The prepared statement.
 */
mysql_stmt query_preparator::prepare_insert_into(
    mysql& ms,
    const std::string& table,
    const std::vector<query_preparator::pb_entry>& mapping,
    bool ignore) {
  assert(_unique.empty());
  absl::flat_hash_map<std::string, int> bind_mapping;
  // Find event info.
  io::event_info const* info(io::events::instance().get_event_info(_event_id));
  if (!info)
    throw msg_fmt(
        "could not prepare insertion query for event of type {}: "
        "event is not registered",
        _event_id);

  // Build query string.
  std::string query;
  if (ignore)
    query = fmt::format("INSERT IGNORE INTO {} (", table);
  else
    query = fmt::format("INSERT INTO {} (", table);

  const mapping::entry* entries = info->get_mapping();
  if (entries)
    throw msg_fmt(
        "prepare_insert_into only works with BBDO with embedded protobuf "
        "message");

  /* Here is the protobuf case : no mapping */
  const google::protobuf::Descriptor* desc =
      google::protobuf::DescriptorPool::generated_pool()->FindMessageTypeByName(
          fmt::format("com.centreon.broker.{}", info->get_name()));

  std::vector<std::tuple<std::string, uint32_t, uint16_t>> pb_mapping;

  int size = 0;
  for (auto& e : mapping) {
    const google::protobuf::FieldDescriptor* f =
        desc->FindFieldByNumber(e.number);
    if (f) {
      if (static_cast<uint32_t>(f->index()) >= pb_mapping.size())
        pb_mapping.resize(f->index() + 1);
      const std::string& entry_name = f->name();
      pb_mapping[f->index()] =
          std::make_tuple(entry_name, e.max_length, e.attribute);
      query.append(entry_name);
      query.append(",");
      bind_mapping.emplace(fmt::format(":{}", entry_name), size++);
    } else
      throw msg_fmt(
          "Protobuf field with number {} does not exist in message '{}'",
          e.number, info->get_name());
  }
  query.resize(query.size() - 1);
  query.append(") VALUE(");
  query.reserve(query.size() + 2 * size);
  for (int i = 0; i < size; i++)
    query.append("?,");
  query.resize(query.size() - 1);
  query.append(")");

  _logger->debug("mysql: query_preparator: {}", query);
  // Prepare statement.
  mysql_stmt retval(_logger);
  try {
    retval = ms.prepare_query(query, bind_mapping);
    retval.set_pb_mapping(std::move(pb_mapping));
  } catch (std::exception const& e) {
    throw msg_fmt(
        "could not prepare insertion query for event '{}' in table '{}': {}",
        info->get_name(), info->get_table_v2(), e.what());
  }
  return retval;
}

/**
 *  Prepare insertion query for specified event.
 *
 *  @param[out] q  Database query, prepared and ready to run.
 *  @param[in] ignore  A boolean telling if the query ignores errors
 *                      (default value is false).
 */
mysql_stmt query_preparator::prepare_insert(mysql& ms, bool ignore) {
  absl::flat_hash_map<std::string, int> bind_mapping;
  // Find event info.
  io::event_info const* info(io::events::instance().get_event_info(_event_id));
  if (!info)
    throw msg_fmt(
        "could not prepare insertion query for event of type {}: "
        "event is not registered",
        _event_id);

  // Build query string.
  std::string query;
  if (ignore)
    query = "INSERT IGNORE INTO ";
  else
    query = "INSERT INTO ";

  query.append(info->get_table_v2());
  query.append(" (");
  const mapping::entry* entries = info->get_mapping();
  if (entries) {
    for (int i = 0; !entries[i].is_null(); ++i) {
      char const* entry_name;
      entry_name = entries[i].get_name_v2();
      if (!entry_name || !entry_name[0] ||
          (_excluded.find(entry_name) != _excluded.end()))
        continue;
      query.append(entry_name);
      query.append(", ");
    }
    query.resize(query.size() - 2);
    query.append(") VALUES(");
    int size = 0;
    for (int i = 0; !entries[i].is_null(); ++i) {
      char const* entry_name;
      entry_name = entries[i].get_name_v2();
      if (!entry_name || !entry_name[0] ||
          (_excluded.find(entry_name) != _excluded.end()))
        continue;
      bind_mapping.insert(
          std::make_pair(fmt::format(":{}", entry_name), size++));
      query.append("?,");
    }
    query.resize(query.size() - 1);
    query.append(")");

  } else {
    /* Here is the protobuf case : no mapping */
    const google::protobuf::Descriptor* desc =
        google::protobuf::DescriptorPool::generated_pool()
            ->FindMessageTypeByName(
                fmt::format("com.centreon.broker.{}", info->get_name()));

    int size = 0;
    for (int i = 0; i < desc->field_count(); i++) {
      // log_v2::neb()->info("{}", desc->field(i)->name());
      const std::string entry_name = desc->field(i)->name();
      query.append(entry_name);
      query.append(",");
      bind_mapping.insert(
          std::make_pair(fmt::format(":{}", entry_name), size++));
    }
    query.resize(query.size() - 1);
    query.append(") VALUE(");
    for (int i = 0; i < size; i++)
      query.append("?,");

    query.resize(query.size() - 1);
    query.append(")");
  }
  _logger->debug("mysql: query_preparator: {}", query);
  // Prepare statement.
  mysql_stmt retval(_logger);
  try {
    retval = ms.prepare_query(query, bind_mapping);
  } catch (std::exception const& e) {
    throw msg_fmt(
        "could not prepare insertion query for event '{}' in table '{}': {}",
        info->get_name(), info->get_table_v2(), e.what());
  }
  return retval;
}

mysql_stmt query_preparator::prepare_insert_or_update(mysql& ms) {
  absl::flat_hash_map<std::string, int> insert_bind_mapping;
  absl::flat_hash_map<std::string, int> update_bind_mapping;
  // Find event info.
  io::event_info const* info(io::events::instance().get_event_info(_event_id));
  if (!info)
    throw msg_fmt(
        "could not prepare insertion query for event of type {} : "
        "event is not registered ",
        _event_id);

  // Build query string.
  std::string insert("INSERT INTO ");
  std::string update(" ON DUPLICATE KEY UPDATE ");
  insert.append(info->get_table_v2());
  insert.append(" (");
  mapping::entry const* entries(info->get_mapping());
  for (int i(0); !entries[i].is_null(); ++i) {
    char const* entry_name;
    entry_name = entries[i].get_name_v2();
    if (!entry_name || !entry_name[0] ||
        (_excluded.find(entry_name) != _excluded.end()))
      continue;
    insert.append(entry_name);
    insert.append(",");
  }
  insert.resize(insert.size() - 1);
  insert.append(") VALUES(");
  std::string key;
  int insert_size(0);
  int update_size(0);
  for (int i(0); !entries[i].is_null(); ++i) {
    char const* entry_name;
    entry_name = entries[i].get_name_v2();
    if (!entry_name || !entry_name[0] ||
        (_excluded.find(entry_name) != _excluded.end()))
      continue;
    key = std::string(":");
    key.append(entry_name);
    if (_unique.find(entry_name) == _unique.end()) {
      update.append(entry_name);
      insert.append("?,");
      update.append("=?,");
      key.append("1");
      insert_bind_mapping.insert(std::make_pair(key, insert_size++));
      key[key.size() - 1] = '2';
      update_bind_mapping.insert(std::make_pair(key, update_size++));
    } else {
      insert.append("?,");
      insert_bind_mapping.insert(std::make_pair(key, insert_size++));
    }
  }
  insert.resize(insert.size() - 1);
  update.resize(update.size() - 1);
  insert.append(") ");
  insert.append(update);

  for (absl::flat_hash_map<std::string, int>::const_iterator
           it(update_bind_mapping.begin()),
       end(update_bind_mapping.end());
       it != end; ++it)
    insert_bind_mapping.insert(
        std::make_pair(it->first, it->second + insert_size));

  _logger->debug("mysql: query_preparator: {}", insert);
  // Prepare statement.
  mysql_stmt retval(_logger);
  try {
    retval = ms.prepare_query(insert, insert_bind_mapping);
  } catch (std::exception const& e) {
    throw msg_fmt(
        "could not prepare insert or update query for event '{}' in table "
        "'{}': {}",
        info->get_name(), info->get_table_v2(), e.what());
  }
  return retval;
}

mysql_stmt query_preparator::prepare_insert_or_update_table(
    mysql& ms,
    const std::string& table,
    const std::vector<query_preparator::pb_entry>& mapping) {
  assert(_unique.empty());
  absl::flat_hash_map<std::string, int> insert_bind_mapping;
  absl::flat_hash_map<std::string, int> update_bind_mapping;
  // Find event info.
  io::event_info const* info(io::events::instance().get_event_info(_event_id));
  if (!info)
    throw msg_fmt(
        "could not prepare insertion query for event of type {} : "
        "event is not registered ",
        _event_id);

  // Build query string.
  std::string insert{fmt::format("INSERT INTO {} (", table)};
  std::string update(" ON DUPLICATE KEY UPDATE ");

  const mapping::entry* entries = info->get_mapping();
  if (entries)
    throw msg_fmt(
        "prepare_insert_or_update_table only works with BBDO with embedded "
        "protobuf message");

  const google::protobuf::Descriptor* desc =
      google::protobuf::DescriptorPool::generated_pool()->FindMessageTypeByName(
          fmt::format("com.centreon.broker.{}", info->get_name()));

  assert(desc);

  std::vector<std::tuple<std::string, uint32_t, uint16_t>> pb_mapping;

  std::string key;
  for (auto& e : mapping) {
    const google::protobuf::FieldDescriptor* f =
        desc->FindFieldByNumber(e.number);
    if (f) {
      if (static_cast<uint32_t>(f->index()) >= pb_mapping.size())
        pb_mapping.resize(f->index() + 1);
      const std::string& entry_name = f->name();
      pb_mapping[f->index()] =
          std::make_tuple(entry_name, e.max_length, e.attribute);
      if (entry_name.empty() || _excluded.find(entry_name) != _excluded.end())
        continue;
      insert.append(e.name);
      insert.append(",");
    } else
      throw msg_fmt(
          "Protobuf field with number {} does not exist in message '{}'",
          e.number, info->get_name());
  }
  insert.resize(insert.size() - 1);
  insert.append(") VALUES(");
  int insert_size(0);
  int update_size(0);
  for (auto& e : mapping) {
    const google::protobuf::FieldDescriptor* f =
        desc->FindFieldByNumber(e.number);
    if (f) {
      const std::string& entry_name = f->name();
      if (entry_name.empty() || _excluded.find(entry_name) != _excluded.end())
        continue;
      key = fmt::format(":{}", entry_name);
      if (std::find_if(_pb_unique.begin(), _pb_unique.end(),
                       [nb = e.number](const query_preparator::pb_entry& p) {
                         return p.number == nb;
                       }) == _pb_unique.end()) {
        insert.append("?,");
        update.append(fmt::format("{}=?,", e.name));
        key.append("1");
        insert_bind_mapping.insert(std::make_pair(key, insert_size++));
        key[key.size() - 1] = '2';
        update_bind_mapping.insert(std::make_pair(key, update_size++));
      } else {
        insert.append("?,");
        insert_bind_mapping.insert(std::make_pair(key, insert_size++));
      }
    } else
      throw msg_fmt(
          "Protobuf field with number {} does not exist in message '{}'",
          e.number, info->get_name());
  }
  insert.resize(insert.size() - 1);
  update.resize(update.size() - 1);
  insert.append(") ");
  insert.append(update);

  for (auto it = update_bind_mapping.begin(), end = update_bind_mapping.end();
       it != end; ++it)
    insert_bind_mapping.insert(
        std::make_pair(it->first, it->second + insert_size));

  _logger->debug("mysql: query_preparator: {}", insert);
  // Prepare statement.
  mysql_stmt retval(_logger);
  try {
    retval = ms.prepare_query(insert, insert_bind_mapping);
    retval.set_pb_mapping(std::move(pb_mapping));
  } catch (std::exception const& e) {
    throw msg_fmt(
        "could not prepare insert or update query for event '{}' in table "
        "'{}': {}",
        info->get_name(), table, e.what());
  }
  return retval;
}

/**
 *  Prepare update query for specified event.
 *
 *  @param[out] q  Database query, prepared and ready to run.
 */
mysql_stmt query_preparator::prepare_update(mysql& ms) {
  absl::flat_hash_map<std::string, int> query_bind_mapping;
  absl::flat_hash_map<std::string, int> where_bind_mapping;
  // Find event info.
  io::event_info const* info(io::events::instance().get_event_info(_event_id));
  if (!info)
    throw msg_fmt(
        "could not prepare update query for event of type {}:"
        "event is not registered",
        _event_id);

  // Build query string.
  std::string query("UPDATE ");
  std::string where(" WHERE ");
  query.append(info->get_table_v2());
  query.append(" SET ");
  mapping::entry const* entries(info->get_mapping());
  std::string key;
  int query_size(0);
  int where_size(0);
  for (int i = 0; !entries[i].is_null(); ++i) {
    char const* entry_name;
    entry_name = entries[i].get_name_v2();
    if (!entry_name || !entry_name[0] ||
        (_excluded.find(entry_name) != _excluded.end()))
      continue;
    // Standard field.
    if (_unique.find(entry_name) == _unique.end()) {
      query.append(entry_name);
      key = std::string(":");
      key.append(entry_name);
      query.append("=?,");
      query_bind_mapping.insert(std::make_pair(key, query_size++));
    }
    // Part of ID field.
    else {
      where.append(entry_name);
      where.append("=? AND ");
      key = std::string(":");
      key.append(entry_name);
      where_bind_mapping.insert(std::make_pair(key, where_size++));
    }
  }
  query.resize(query.size() - 1);
  query.append(where, 0, where.size() - 5);

  for (absl::flat_hash_map<std::string, int>::iterator
           it(where_bind_mapping.begin()),
       end(where_bind_mapping.end());
       it != end; ++it)
    query_bind_mapping.insert(
        std::make_pair(it->first, it->second + query_size));

  // Prepare statement.
  mysql_stmt retval(_logger);
  try {
    retval = ms.prepare_query(query, query_bind_mapping);
  } catch (std::exception const& e) {
    throw msg_fmt(
        "could not prepare update query for event '{}': on table '{}': {}",
        info->get_name(), info->get_table_v2(), e.what());
  }
  return retval;
}

/**
 *  Prepare update query for specified event.
 *
 *  @param[out] q  Database query, prepared and ready to run.
 */
mysql_stmt query_preparator::prepare_update_table(
    mysql& ms,
    const std::string& table,
    const std::vector<pb_entry>& mapping) {
  assert(_unique.empty());
  absl::flat_hash_map<std::string, int> query_bind_mapping;
  absl::flat_hash_map<std::string, int> where_bind_mapping;
  // Find event info.
  io::event_info const* info(io::events::instance().get_event_info(_event_id));
  if (!info)
    throw msg_fmt(
        "could not prepare update query for event of type {}:"
        "event is not registered",
        _event_id);

  // Build query string.
  std::string query(fmt::format("UPDATE {} SET ", table));
  std::string where(" WHERE ");
  const mapping::entry* entries(info->get_mapping());
  if (entries)
    throw msg_fmt(
        "prepare_update_table only works with BBDO with embedded protobuf "
        "message");
  std::string key;
  int query_size(0);
  int where_size(0);

  /* Here is the protobuf case : no mapping */
  const google::protobuf::Descriptor* desc =
      google::protobuf::DescriptorPool::generated_pool()->FindMessageTypeByName(
          fmt::format("com.centreon.broker.{}", info->get_name()));

  std::vector<std::tuple<std::string, uint32_t, uint16_t>> pb_mapping;
  for (auto& e : mapping) {
    const google::protobuf::FieldDescriptor* f =
        desc->FindFieldByNumber(e.number);
    if (f) {
      if (static_cast<uint32_t>(f->index()) >= pb_mapping.size())
        pb_mapping.resize(f->index() + 1);
      const std::string& entry_name = f->name();
      pb_mapping[f->index()] =
          std::make_tuple(entry_name, e.max_length, e.attribute);
      // Standard field.
      if (std::find_if(_pb_unique.begin(), _pb_unique.end(),
                       [nb = e.number](const query_preparator::pb_entry& p) {
                         return p.number == nb;
                       }) == _pb_unique.end()) {
        query.append(e.name);
        key = fmt::format(":{}", entry_name);
        query.append("=?,");
        query_bind_mapping.insert(std::make_pair(key, query_size++));
      }
      // Part of ID field.
      else {
        where.append(e.name);
        where.append("=? AND ");
        key = fmt::format(":{}", entry_name);
        where_bind_mapping.insert(std::make_pair(key, where_size++));
      }
    } else
      throw msg_fmt(
          "could not prepare update query for event of type {}:"
          "protobuf field with number {} does not exist in '{}' protobuf "
          "object",
          _event_id, e.number, info->get_name());
  }
  query.resize(query.size() - 1);
  query.append(where, 0, where.size() - 5);

  for (auto it = where_bind_mapping.begin(), end = where_bind_mapping.end();
       it != end; ++it)
    query_bind_mapping.insert(
        std::make_pair(it->first, it->second + query_size));

  // Prepare statement.
  mysql_stmt retval(_logger);
  try {
    retval = ms.prepare_query(query, query_bind_mapping);
    retval.set_pb_mapping(std::move(pb_mapping));
  } catch (std::exception const& e) {
    throw msg_fmt(
        "could not prepare update query for event '{}': on table '{}': {}",
        info->get_name(), info->get_table_v2(), e.what());
  }
  return retval;
}

/**
 *  Prepare deletion query for specified event.
 *
 *  @param[out] q  Database query, prepared and ready to run.
 */
mysql_stmt query_preparator::prepare_delete(mysql& ms) {
  absl::flat_hash_map<std::string, int> bind_mapping;
  // Find event info.
  io::event_info const* info(io::events::instance().get_event_info(_event_id));
  if (!info)
    throw msg_fmt(
        "could not prepare deletion query for event of type "
        " {}: event is not registered",
        _event_id);

  // Prepare query.
  std::string query("DELETE FROM ");
  query.append(info->get_table_v2());
  query.append(" WHERE ");
  int size = 0;
  for (event_unique::const_iterator it(_unique.begin()), end(_unique.end());
       it != end; ++it) {
    query.append("(");
    query.append(*it);
    query.append("=?");
    std::string key(":");
    key.append(*it);
    bind_mapping.insert(std::make_pair(key, size++));

    query.append(") AND ");
  }
  query.resize(query.size() - 5);

  // Prepare statement.
  mysql_stmt retval(_logger);
  try {
    retval = ms.prepare_query(query, bind_mapping);
  } catch (std::exception const& e) {
    // FIXME DBR
    throw msg_fmt(
        "could not prepare deletion query for event '{}' on table '{}': {}",
        info->get_name(), info->get_table_v2(), e.what());
  }
  return retval;
}

mysql_stmt query_preparator::prepare_delete_table(mysql& ms,
                                                  const std::string& table) {
  assert(_unique.empty());
  absl::flat_hash_map<std::string, int> bind_mapping;
  // Find event info.
  io::event_info const* info(io::events::instance().get_event_info(_event_id));
  if (!info)
    throw msg_fmt(
        "could not prepare delete query for event of type {}: event is not "
        "registered",
        _event_id);

  // Build query string.
  std::string query{fmt::format("DELETE FROM {} WHERE ", table)};

  const mapping::entry* entries = info->get_mapping();
  if (entries)
    throw msg_fmt(
        "prepare_delete_table only works with BBDO with embedded protobuf "
        "message");

  /* Here is the protobuf case : no mapping */
  const google::protobuf::Descriptor* desc =
      google::protobuf::DescriptorPool::generated_pool()->FindMessageTypeByName(
          fmt::format("com.centreon.broker.{}", info->get_name()));

  std::vector<std::tuple<std::string, uint32_t, uint16_t>> pb_mapping;

  int size = 0;
  for (auto& u : _pb_unique) {
    const google::protobuf::FieldDescriptor* f =
        desc->FindFieldByNumber(u.number);
    if (f) {
      if (static_cast<uint32_t>(f->index()) >= pb_mapping.size())
        pb_mapping.resize(f->index() + 1);
      const std::string& entry_name = f->name();
      pb_mapping[f->index()] =
          std::make_tuple(u.name, u.max_length, u.attribute);
      query.append(fmt::format("{}=? AND ", u.name));
      bind_mapping.emplace(fmt::format(":{}", entry_name), size++);
    } else
      throw msg_fmt(
          "Protobuf field at number {} does not exist in message '{}'",
          u.number, info->get_name());
  }
  query.resize(query.size() - 5);

  _logger->debug("mysql: query_preparator: {}", query);
  // Prepare statement.
  mysql_stmt retval(_logger);
  try {
    retval = ms.prepare_query(query, bind_mapping);
    retval.set_pb_mapping(std::move(pb_mapping));
  } catch (std::exception const& e) {
    throw msg_fmt(
        "could not prepare delete query for event '{}' in table '{}': {}",
        info->get_name(), table, e.what());
  }
  return retval;
}
