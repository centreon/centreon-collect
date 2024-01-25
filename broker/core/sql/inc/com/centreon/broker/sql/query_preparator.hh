/*
** Copyright 2015, 2021 Centreon
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

#ifndef CCB_QUERY_PREPARATOR_HH
#define CCB_QUERY_PREPARATOR_HH

#include "com/centreon/broker/sql/mysql.hh"

namespace com::centreon::broker {

/**
 *  @class query_preparator query_preparator.hh
 * "com/centreon/broker/sql/query_preparator.hh"
 *  @brief Prepare database queries.
 *
 *  Prepare queries using event mappings.
 *
 *  A query preparator is attached to a BBDO type element. It is constructed
 *  with the BBDO static type.
 *
 *  A second important point is the event_unique object that is used by the
 *  query preparator to know the unique key to use for inserts, updates...
 *  It is also provided in the constructor and if it is not defined, we use
 *  an empty one.
 *
 *  Once, the query preparator constructed, we can use it to prepare:
 *  * an insert statement
 *  * an update statement
 *  * an insert or update statement: which tries to insert the row and on
 *    duplication, updates it.
 *  * a delete statement.
 *
 *
 *  Here is an example of the query preparator usage:
 *
 *   query_preparator::event_unique unique;
 *   unique.insert("hostgroup_id");
 *   query_preparator qp(neb::host_group::static_type(), unique);
 *   auto host_group_insupdate = qp.prepare_update(_mysql);
 *
 *  In that example, we prepare a statement for a neb::host_group BBDO object.
 *  The prepared query is an update with a WHERE condition on hostgroup_id.
 *
 *  To use it, we can just do something like this:
 *
 *   host_group_insupdate << hg;
 *
 *   Where hg is of type neb::host_group.
 */
class query_preparator {
 public:
  using excluded_fields = absl::btree_set<std::string>;
  using doubled_fields = absl::btree_set<std::string>;
  using event_unique = absl::btree_set<std::string>;

  struct pb_entry {
    int32_t number;
    const char* name;
    uint16_t attribute;
    uint32_t max_length;
  };

  using event_pb_unique = std::vector<pb_entry>;

 private:
  uint32_t _event_id;
  excluded_fields _excluded;
  event_unique _unique;
  event_pb_unique _pb_unique;

 public:
  query_preparator(uint32_t event_id,
                   event_unique const& unique = event_unique(),
                   excluded_fields const& excluded = excluded_fields());
  query_preparator(uint32_t event_id, const event_pb_unique& pb_unique);
  ~query_preparator() noexcept = default;
  query_preparator(query_preparator const&) = delete;
  query_preparator& operator=(query_preparator const&) = delete;
  database::mysql_stmt prepare_insert(mysql& q, bool ignore = false);
  database::mysql_stmt prepare_insert_into(mysql& q,
                                           const std::string& table,
                                           const std::vector<pb_entry>& mapping,
                                           bool ignore = false);
  database::mysql_stmt prepare_update(mysql& q);
  database::mysql_stmt prepare_update_table(
      mysql& q,
      const std::string& table,
      const std::vector<pb_entry>& mapping);
  database::mysql_stmt prepare_insert_or_update(mysql& ms);
  database::mysql_stmt prepare_insert_or_update_table(
      mysql& ms,
      const std::string& table,
      const std::vector<query_preparator::pb_entry>& mapping);
  database::mysql_stmt prepare_delete(mysql& q);
  database::mysql_stmt prepare_delete_table(mysql& ms,
                                            const std::string& table);
};

}

#endif  // !CCB_QUERY_PREPARATOR_HH
