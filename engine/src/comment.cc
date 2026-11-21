/**
 * Copyright 1999-2010 Ethan Galstad
 * Copyright 2011-2026 Centreon
 *
 * This file is part of Centreon Engine.
 *
 * Centreon Engine is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation.
 *
 * Centreon Engine is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Centreon Engine. If not, see
 * <http://www.gnu.org/licenses/>.
 */

#include "com/centreon/engine/broker.hh"

using namespace com::centreon::engine;

uint64_t comment::_next_comment_id = 1LLU;

uint64_t comment::get_next_comment_id() {
  return _next_comment_id;
}

void comment::set_next_comment_id(uint64_t next_comment_id) {
  _next_comment_id = next_comment_id;
}

comment::comment(comment::type comment_type,
                 comment::e_type entry_type,
                 uint64_t host_id,
                 uint64_t service_id,
                 time_t entry_time,
                 std::string const& author,
                 std::string const& comment_data,
                 bool persistent,
                 comment::src source,
                 bool expires,
                 time_t expire_time,
                 uint64_t comment_id)
    : _comment_type{comment_type},
      _entry_type{entry_type},
      _comment_id{comment_id},
      _source{source},
      _persistent{persistent},
      _entry_time{entry_time},
      _expires{expires},
      _expire_time{expire_time},
      _host_id{host_id},
      _service_id{service_id},
      _author{author},
      _comment_data{comment_data} {
  /* When no id is supplied the comment is newly created: mint a monotonic id
   * (the counter is persisted in retention as next_comment_id) and notify
   * Broker. When an id is supplied the comment is being reloaded, so nothing is
   * emitted. Comments are no longer kept in memory: the object is transient and
   * only exists to carry the creation event. */
  if (!comment_id) {
    _comment_id = _next_comment_id++;
    broker_comment_data(NEBTYPE_COMMENT_ADD, _comment_type, _entry_type,
                        _host_id, _service_id, _entry_time, _author.c_str(),
                        _comment_data.c_str(), _persistent, _source, _expires,
                        _expire_time, _comment_id);
  }
}

/**
 * @brief deletes a host or service comment from its id.
 *
 * Broker matches the deletion on (internal_id, instance_id), so we only need
 * the comment id: the full tuple no longer has to be rebuilt and Engine keeps
 * no comment in memory anymore.
 *
 * @param comment_id The comment id.
 *
 * @return True on success, False otherwise.
 */
bool comment::delete_comment(uint64_t comment_id) {
  if (comment_id == 0)
    return false;

  broker_comment_data(NEBTYPE_COMMENT_DELETE, comment::host, comment::user, 0,
                      0, 0, nullptr, nullptr, false, comment::internal, false,
                      0, comment_id);
  return true;
}

void comment::delete_host_comments(uint64_t host_id) {
  /* Bulk deletion: a single event tells Broker to delete every host comment of
   * this host (matched by host_id + instance_id). comment_id 0 is the sentinel
   * for a delete-by-target rather than a delete-by-id. The full-tuple is no
   * longer needed, so no map iteration. */
  broker_comment_data(NEBTYPE_COMMENT_DELETE, comment::host, comment::user,
                      host_id, 0, 0, nullptr, nullptr, false, comment::internal,
                      false, 0, 0);
}

/**
 * @brief Deletes comments of the given service.
 *
 * @param host_id Id of the service's host.
 * @param service_id Id of the service.
 */
void comment::delete_service_comments(uint64_t host_id, uint64_t service_id) {
  /* Bulk deletion of every comment of this service (host_id + service_id +
   * instance_id). comment_id 0 signals a delete-by-target. */
  broker_comment_data(NEBTYPE_COMMENT_DELETE, comment::service, comment::user,
                      host_id, service_id, 0, nullptr, nullptr, false,
                      comment::internal, false, 0, 0);
}

comment::type comment::get_comment_type() const {
  return _comment_type;
}

comment::e_type comment::get_entry_type() const {
  return _entry_type;
}

uint64_t comment::get_comment_id() const {
  return _comment_id;
}

comment::src comment::get_source() const {
  return _source;
}

bool comment::get_persistent() const {
  return _persistent;
}

time_t comment::get_entry_time() const {
  return _entry_time;
}

bool comment::get_expires() const {
  return _expires;
}

time_t comment::get_expire_time() const {
  return _expire_time;
}

uint64_t comment::get_host_id() const {
  return _host_id;
}

uint64_t comment::get_service_id() const {
  return _service_id;
}

std::string const& comment::get_author() const {
  return _author;
}

std::string const& comment::get_comment_data() const {
  return _comment_data;
}

/**
 *  Equal operator.
 *
 *  @param[in] obj1 The first object to compare.
 *  @param[in] obj2 The second object to compare.
 *
 *  @return True if is the same object, otherwise false.
 */
bool comment::operator==(comment const& obj) throw() {
  return (
      _comment_type == obj.get_comment_type() &&
      _entry_type == obj.get_entry_type() &&
      _comment_id == obj.get_comment_id() && _source == obj.get_source() &&
      _persistent == obj.get_persistent() &&
      _entry_time == obj.get_entry_time() && _expires == obj.get_expires() &&
      _expire_time == obj.get_expire_time() && _host_id == obj.get_host_id() &&
      _service_id == obj.get_service_id() && _author == obj.get_author() &&
      _comment_data == obj.get_comment_data());
}

/**
 *  Not equal operator.
 *
 *  @param[in] obj1 The first object to compare.
 *  @param[in] obj2 The second object to compare.
 *
 *  @return True if is not the same object, otherwise false.
 */
bool comment::operator!=(comment const& obj) throw() {
  return !(*this == obj);
}

/**
 *  Dump downtime content into the stream.
 *
 *  @param[out] os  The output stream.
 *  @param[in]  obj The downtime to dump.
 *
 *  @return The output stream.
 */
std::ostream& operator<<(std::ostream& os, comment const& obj) {
  os << "comment {\n"
        "  comment_type:        "
     << obj.get_comment_type()
     << "\n"
        "  entry_type:          "
     << obj.get_entry_type()
     << "\n"
        "  comment_id:          "
     << obj.get_comment_id()
     << "\n"
        "  source:              "
     << obj.get_source()
     << "\n"
        "  persistent:          "
     << obj.get_persistent()
     << "\n"
        "  entry_time:          "
     << obj.get_entry_time()
     << "\n"
        "  expires:             "
     << obj.get_expires()
     << "\n"
        "  expire_time:         "
     << obj.get_expire_time()
     << "\n"
        "  host_id:           "
     << obj.get_host_id()
     << "\n"
        "  service_id: "
     << obj.get_service_id()
     << "\n"
        "  author:              "
     << obj.get_author()
     << "\n"
        "  comment_data:        "
     << obj.get_comment_data()
     << "\n"
        "}\n";
  return (os);
}
