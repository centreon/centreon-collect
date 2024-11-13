/**
 * Copyright 2023 Centreon
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

#include "com/centreon/broker/cache/global_cache_data.hh"

using namespace com::centreon::broker::cache;

void global_cache_data::managed_map(bool create) {
  global_cache::managed_map(create);
  _metric_info = _file->find_or_construct<id_to_metric_info>("metric_info")(
      _file->get_segment_manager());
  _metric_info_allocator = _file->find_or_construct<metric_info_allocator>(
      "metric_info_allocator")(_file->get_segment_manager());
  _index_id_mapping = _file->find_or_construct<index_id_mapping>(
      "index_id_mapping")(_file->get_segment_manager());
  _id_to_host = _file->find_or_construct<id_to_host>("id_to_host")(
      _file->get_segment_manager());
  _id_to_service = _file->find_or_construct<id_to_serv>("id_to_service")(
      _file->get_segment_manager());
  _id_to_instance = _file->find_or_construct<id_to_string>("id_to_instance")(
      _file->get_segment_manager());
  _host_group = _file->find_or_construct<host_group>("host_group")(
      _file->get_segment_manager());
  _service_group = _file->find_or_construct<service_group>("service_group")(
      _file->get_segment_manager());
  _id_to_tag = _file->find_or_construct<id_to_tag>("id_to_tag")(
      _file->get_segment_manager());
  _host_tag = _file->find_or_construct<host_tag>("host_tag")(
      _file->get_segment_manager());
  _serv_tag = _file->find_or_construct<service_tag>("serv_tag")(
      _file->get_segment_manager());
}

/***********************************************************/
/*                   feeders                               */
/***********************************************************/

/**
 * @brief add metric infos to cache
 *
 * @param metric_id
 * @param index_id
 * @param name
 * @param unit
 * @param min
 * @param max
 */
void global_cache_data::set_metric_info(uint64_t metric_id,
                                        uint64_t index_id,
                                        const std::string_view& name,
                                        const std::string_view& unit,
                                        double min,
                                        double max) {
  try {
    absl::WriterMutexLock l(&_protect);
    auto exist = _metric_info->find(metric_id);
    if (exist != _metric_info->end()) {
      metric_info& to_update = *exist->second;
      if (to_update.name != name) {
        to_update.name.assign(name.data(), name.length());
      }
      if (to_update.unit != unit) {
        to_update.unit.assign(unit.data(), unit.length());
      }
      to_update.min = min;
      to_update.max = max;
      to_update.index_id = index_id;
    } else {
      if (_metric_info->capacity() <= _metric_info->size()) {  // need to grow ?
        _metric_info->reserve(_metric_info->capacity() + 0x80000);
      }
      metric_info* to_add = _metric_info_allocator->allocate();
      to_add = new (to_add) metric_info(index_id, name, unit, min, max,
                                        _file->get_segment_manager());

      _metric_info->emplace(metric_id, to_add);
    }
  } catch (const interprocess::bad_alloc& e) {
    SPDLOG_LOGGER_DEBUG(_logger, "file full => grow");
    allocation_exception_handler();
    set_metric_info(metric_id, index_id, name, unit, min, max);
  }
}

/**
 * @brief store instance or poller name
 *
 * @param instance_id
 * @param instance_name
 */
void global_cache_data::store_instance(uint64_t instance_id,
                                       const std::string_view& instance_name) {
  try {
    absl::WriterMutexLock l(&_protect);
    auto exist = _id_to_instance->find(instance_id);
    if (exist == _id_to_instance->end()) {
      if (_id_to_instance->size() ==
          _id_to_instance->capacity()) {  // need to grow?
        _id_to_instance->reserve(_id_to_instance->capacity() + 0x100);
      }
      _id_to_instance->emplace(
          instance_id, string(instance_name.data(), instance_name.length(),
                              _file->get_segment_manager()));
    } else if (instance_name.compare(0, exist->second.length(),
                                     exist->second.c_str())) {
      exist->second.assign(instance_name.data(), instance_name.length());
    }
  } catch (const interprocess::bad_alloc& e) {
    SPDLOG_LOGGER_DEBUG(_logger, "file full => grow");
    allocation_exception_handler();
    store_instance(instance_id, instance_name);
  }
}

/**
 * @brief store host
 *
 * @param host_id
 * @param host_name
 * @param resource_id
 * @param severity_id
 */
void global_cache_data::store_host(uint64_t host_id,
                                   const std::string_view& host_name,
                                   uint64_t resource_id,
                                   uint64_t severity_id) {
  try {
    absl::WriterMutexLock l(&_protect);
    auto exist = _id_to_host->find(host_id);
    if (exist == _id_to_host->end()) {
      if (_id_to_host->size() == _id_to_host->capacity()) {  // need to grow?
        _id_to_host->reserve(_id_to_host->capacity() + 0x1000);
      }
      _id_to_host->emplace(host_id,
                           resource_info(host_name, resource_id, severity_id,
                                         _file->get_segment_manager()));
    } else {
      if (host_name.compare(0, exist->second.name.length(),
                            exist->second.name.c_str())) {
        exist->second.name.assign(host_name.data(), host_name.length());
      }
      exist->second.resource_id = resource_id;
      exist->second.severity_id = severity_id;
    }
  } catch (const interprocess::bad_alloc& e) {
    SPDLOG_LOGGER_DEBUG(_logger, "file full => grow");
    allocation_exception_handler();
    store_host(host_id, host_name, resource_id, severity_id);
  }
}

/**
 * @brief sotre service in cache
 *
 * @param host_id
 * @param service_id
 * @param service_description
 * @param resource_id
 * @param severity_id
 */
void global_cache_data::store_service(
    uint64_t host_id,
    uint64_t service_id,
    const std::string_view& service_description,
    uint64_t resource_id,
    uint64_t severity_id) {
  try {
    absl::WriterMutexLock l(&_protect);
    auto exist = _id_to_service->find({host_id, service_id});
    if (exist == _id_to_service->end()) {
      if (_id_to_service->size() ==
          _id_to_service->capacity()) {  // need to grow?
        _id_to_service->reserve(_id_to_service->capacity() + 0x10000);
      }
      _id_to_service->emplace(
          host_serv_pair(host_id, service_id),
          resource_info(service_description, resource_id, severity_id,
                        _file->get_segment_manager()));
    } else {
      if (service_description.compare(0, exist->second.name.length(),
                                      exist->second.name.c_str())) {
        exist->second.name.assign(service_description.data(),
                                  service_description.length());
      }
      exist->second.resource_id = resource_id;
      exist->second.severity_id = severity_id;
    }

  } catch (const interprocess::bad_alloc& e) {
    SPDLOG_LOGGER_DEBUG(_logger, "file full => grow");
    allocation_exception_handler();
    store_service(host_id, service_id, service_description, resource_id,
                  severity_id);
  }
}

/**
 * @brief sotre index id <=> (host_id, service_id)
 *
 * @param index_id
 * @param host_id
 * @param service_id
 */
void global_cache_data::set_index_mapping(uint64_t index_id,
                                          uint64_t host_id,
                                          uint64_t service_id) {
  try {
    absl::WriterMutexLock l(&_protect);
    if (_index_id_mapping->size() ==
        _index_id_mapping->capacity()) {  // need to grow
      _index_id_mapping->reserve(_index_id_mapping->size() + 0x10000);
    }
    _index_id_mapping->emplace(index_id, host_serv_pair(host_id, service_id));
  } catch (const interprocess::bad_alloc& e) {
    SPDLOG_LOGGER_DEBUG(_logger, "file full => grow");
    allocation_exception_handler();
    set_index_mapping(index_id, host_id, service_id);
  }
}

/**
 * @brief add a host member into a host group
 *
 * @param group
 * @param host
 * @param poller_id
 */
void global_cache_data::add_host_to_group(uint64_t group,
                                          uint64_t host,
                                          uint64_t poller_id) {
  try {
    absl::WriterMutexLock l(&_protect);
    _host_group->emplace(host_group_element{host, group, poller_id});
  } catch (const interprocess::bad_alloc& e) {
    SPDLOG_LOGGER_DEBUG(_logger, "file full => grow");
    allocation_exception_handler();
    add_host_to_group(group, host, poller_id);
  }
}

/**
 * @brief remove host from a host group
 *
 * @param group
 * @param host
 */
void global_cache_data::remove_host_from_group(uint64_t group, uint64_t host) {
  absl::WriterMutexLock l(&_protect);
  _host_group->get<2>().erase(host_group_element{host, group, 0});
}

/**
 * @brief remove all members of an host group and poller
 *
 * @param group id of the group
 * @param poller_id id of the poller
 */
void global_cache_data::remove_host_group_members(uint64_t group,
                                                  uint64_t poller_id) {
  absl::WriterMutexLock l(&_protect);
  auto range_iters = _host_group->get<1>().equal_range(group);
  while (range_iters.first != range_iters.second) {
    if (range_iters.first->poller_id == poller_id) {
      range_iters.first = _host_group->get<1>().erase(range_iters.first);
    } else {
      ++range_iters.first;
    }
  }
}

/**
 * @brief add (host, service) to a service group
 *
 * @param group
 * @param host
 * @param service
 * @param poller_id
 */
void global_cache_data::add_service_to_group(uint64_t group,
                                             uint64_t host,
                                             uint64_t service,
                                             uint64_t poller_id) {
  try {
    absl::WriterMutexLock l(&_protect);
    _service_group->emplace(
        service_group_element{{host, service}, group, poller_id});
  } catch (const interprocess::bad_alloc& e) {
    SPDLOG_LOGGER_DEBUG(_logger, "file full => grow");
    allocation_exception_handler();
    add_service_to_group(group, host, service, poller_id);
  }
}

/**
 * @brief remove a (host,service) from a service group
 *
 * @param group
 * @param host
 * @param service
 */
void global_cache_data::remove_service_from_group(uint64_t group,
                                                  uint64_t host,
                                                  uint64_t service) {
  absl::WriterMutexLock l(&_protect);
  _service_group->get<2>().erase(
      service_group_element{{host, service}, group, 0});
}

/**
 * @brief remove all members of a service group and a poller
 *
 * @param group id of a group
 * @param poller_id id of the poller
 */
void global_cache_data::remove_service_group_members(uint64_t group,
                                                     uint64_t poller_id) {
  absl::WriterMutexLock l(&_protect);
  auto range_iters = _service_group->get<1>().equal_range(group);
  while (range_iters.first != range_iters.second) {
    if (range_iters.first->poller_id == poller_id) {
      range_iters.first = _service_group->get<1>().erase(range_iters.first);
    } else {
      ++range_iters.first;
    }
  }
}

/**
 * @brief add a tag
 *
 * @param tag_id
 * @param tag_name
 * @param tag_type
 * @param poller_id
 */
void global_cache_data::add_tag(uint64_t tag_id,
                                const std::string_view& tag_name,
                                TagType tag_type,
                                uint64_t poller_id) {
  try {
    absl::WriterMutexLock l(&_protect);
    auto exist = _id_to_tag->find(tag_id);
    if (exist == _id_to_tag->end()) {
      if (_id_to_tag->size() == _id_to_tag->capacity()) {  // need to grow?
        _id_to_tag->reserve(_id_to_tag->capacity() + 0x1000);
      }
      _id_to_tag->emplace(
          tag_id, resource_tag{tag_type,
                               string(tag_name.data(), tag_name.length(),
                                      _file->get_segment_manager()),
                               poller_id});
    } else {
      if (tag_name.compare(0, exist->second.name.length(),
                           exist->second.name.c_str())) {
        exist->second.name.assign(tag_name.data(), tag_name.length());
      }
    }
  } catch (const interprocess::bad_alloc& e) {
    SPDLOG_LOGGER_DEBUG(_logger, "file full => grow");
    allocation_exception_handler();
    add_tag(tag_id, tag_name, tag_type, poller_id);
  }
}

/**
 * @brief remove a tag
 *
 * @param tag_id
 */
void global_cache_data::remove_tag(uint64_t tag_id) {
  absl::WriterMutexLock l(&_protect);
  _id_to_tag->erase(tag_id);
  _host_tag->get<1>().erase(tag_id);
  _serv_tag->get<1>().erase(tag_id);
}

/**
 * @brief add tags to a host
 *
 * @param host
 * @param tag_filler lambda who returns tag ids, it ends when it returns 0
 */
void global_cache_data::set_host_tag(uint64_t host,
                                     tag_id_enumerator&& tag_filler) {
  try {
    absl::WriterMutexLock l(&_protect);
    uint64_t tag_id;
    while ((tag_id = tag_filler())) {
      _host_tag->insert({host, tag_id});
    }
  } catch (const interprocess::bad_alloc& e) {
    SPDLOG_LOGGER_DEBUG(_logger, "file full => grow");
    allocation_exception_handler();
    set_host_tag(host, std::move(tag_filler));
  }
}

/**
 * @brief add tags to a service
 *
 * @param host
 * @param tag_filler lambda who returns tag ids, it ends when it returns 0
 */
void global_cache_data::set_serv_tag(uint64_t host,
                                     uint64_t serv,
                                     tag_id_enumerator&& tag_filler) {
  try {
    absl::WriterMutexLock l(&_protect);
    uint64_t tag_id;
    while ((tag_id = tag_filler())) {
      _serv_tag->insert({{host, serv}, tag_id});
    }
  } catch (const interprocess::bad_alloc& e) {
    SPDLOG_LOGGER_DEBUG(_logger, "file full => grow");
    allocation_exception_handler();
    set_serv_tag(host, serv, std::move(tag_filler));
  }
}

/***********************************************************/
/*                   getters                               */
/***********************************************************/

/**
 * @brief get metric infos
 *
 * @param metric_id
 * @return const metric_info*
 */
const metric_info* global_cache_data::get_metric_info(
    uint32_t metric_id) const {
  auto search = _metric_info->find(metric_id);
  if (search != _metric_info->end()) {
    return search->second.get();
  }
  return nullptr;
}

/**
 * @brief get instance name
 *
 * @param instance_id
 * @return const string*
 */
const string* global_cache_data::get_instance_name(uint64_t instance_id) const {
  auto search = _id_to_instance->find(instance_id);
  if (search != _id_to_instance->end()) {
    return &search->second;
  }
  return nullptr;
}

/**
 * @brief get infos of a host
 *
 * @param host_id
 * @return const resource_info*
 */
const resource_info* global_cache_data::get_host(uint64_t host_id) const {
  auto search = _id_to_host->find(host_id);
  if (search != _id_to_host->end()) {
    return &search->second;
  }
  return nullptr;
}

/**
 * @brief get info of a host
 *
 * @param index_id
 * @return const resource_info*
 */
const resource_info* global_cache_data::get_host_from_index_id(
    uint64_t index_id) const {
  auto host_id_search = _index_id_mapping->find(index_id);
  if (host_id_search == _index_id_mapping->end()) {
    return nullptr;
  }
  return get_host(host_id_search->second.first);
}

/**
 * @brief get infos of a service
 *
 * @param host_id
 * @param service_id
 * @return const resource_info*
 */
const resource_info* global_cache_data::get_service(uint64_t host_id,
                                                    uint64_t service_id) const {
  auto serv_search = _id_to_service->find(host_serv_pair(host_id, service_id));
  if (serv_search != _id_to_service->end()) {
    return &serv_search->second;
  }
  return nullptr;
}

/**
 * @brief get info of a service
 *
 * @param index_id
 * @return const resource_info*
 */
const resource_info* global_cache_data::get_service_from_index_id(
    uint64_t index_id) const {
  auto host_serv_id_search = _index_id_mapping->find(index_id);
  if (host_serv_id_search == _index_id_mapping->end()) {
    return nullptr;
  }
  return get_service(host_serv_id_search->second.first,
                     host_serv_id_search->second.second);
}

/**
 * @brief get host_id service_id from an index_id
 *
 * @param index_id
 * @return const host_serv_pair*
 */
const host_serv_pair* global_cache_data::get_host_serv_id(
    uint64_t index_id) const {
  auto host_serv_id_search = _index_id_mapping->find(index_id);
  if (host_serv_id_search == _index_id_mapping->end()) {
    return nullptr;
  }
  return &host_serv_id_search->second;
}

/**
 * @brief add service groups to a request body
 *
 * @param host
 * @param service
 * @param request_body
 */
void global_cache_data::append_service_group(uint64_t host,
                                             uint64_t service,
                                             std::ostream& request_body) {
  std::set<uint64_t> sorted;
  auto range =
      _service_group->get<0>().equal_range(host_serv_pair{host, service});
  if (range.first != range.second) {
    // in order to avoid cardinality, we sort results
    for (; range.first != range.second; ++range.first) {
      sorted.insert(range.first->group);
    }
  }
  if (!sorted.empty()) {
    std::set<uint64_t>::const_iterator val_iter = sorted.begin();
    request_body << *(val_iter++);
    for (; val_iter != sorted.end(); ++val_iter) {
      request_body << ',' << *val_iter;
    }
  }
}

/**
 * @brief append host groups to a request body
 *
 * @param host
 * @param request_body
 */
void global_cache_data::append_host_group(uint64_t host,
                                          std::ostream& request_body) {
  std::set<uint64_t> sorted;
  auto range = _host_group->get<0>().equal_range(host);
  if (range.first != range.second) {
    // in order to avoid cardinality, we sort results
    for (; range.first != range.second; ++range.first) {
      sorted.insert(range.first->group);
    }
  }
  if (!sorted.empty()) {
    std::set<uint64_t>::const_iterator val_iter = sorted.begin();
    request_body << *(val_iter++);
    for (; val_iter != sorted.end(); ++val_iter) {
      request_body << ',' << *val_iter;
    }
  }
}

/**
 * @brief append tag ids of a host to a request body
 *
 * @param host
 * @param tag_type
 * @param request_body
 */
void global_cache_data::append_host_tag_id(uint64_t host,
                                           TagType tag_type,
                                           std::ostream& request_body) {
  auto tags = _host_tag->get<0>().equal_range(host);
  bool first = true;
  for (; tags.first != tags.second; ++tags.first) {
    id_to_tag::const_iterator tag_search = _id_to_tag->find(tags.first->tag);
    if (tag_search != _id_to_tag->end() &&
        tag_search->second.tag_type == tag_type) {
      if (first) {
        request_body << tags.first->tag;
        first = false;
      } else {
        request_body << ',' << tags.first->tag;
      }
    }
  }
}

/**
 * @brief append tag ids of a service to a request body
 *
 * @param host
 * @param serv
 * @param tag_type
 * @param request_body
 */
void global_cache_data::append_serv_tag_id(uint64_t host,
                                           uint64_t serv,
                                           TagType tag_type,
                                           std::ostream& request_body) {
  auto tags = _serv_tag->get<0>().equal_range(host_serv_pair{host, serv});
  bool first = true;
  for (; tags.first != tags.second; ++tags.first) {
    id_to_tag::const_iterator tag_search = _id_to_tag->find(tags.first->tag);
    if (tag_search != _id_to_tag->end() &&
        tag_search->second.tag_type == tag_type) {
      if (first) {
        request_body << tags.first->tag;
        first = false;
      } else {
        request_body << ',' << tags.first->tag;
      }
    }
  }
}

/**
 * @brief append tag names of a host to a request body
 *
 * @param host
 * @param tag_type
 * @param request_body
 */
void global_cache_data::append_host_tag_name(uint64_t host,
                                             TagType tag_type,
                                             std::ostream& request_body) {
  auto tags = _host_tag->get<0>().equal_range(host);
  bool first = true;
  for (; tags.first != tags.second; ++tags.first) {
    id_to_tag::const_iterator tag_search = _id_to_tag->find(tags.first->tag);
    if (tag_search != _id_to_tag->end() &&
        tag_search->second.tag_type == tag_type) {
      if (first) {
        request_body << tag_search->second.name;
        first = false;
      } else {
        request_body << ',' << tag_search->second.name;
      }
    }
  }
}

/**
 * @brief append tag names of a service to a request body
 *
 * @param host
 * @param serv
 * @param tag_type
 * @param request_body
 */
void global_cache_data::append_serv_tag_name(uint64_t host,
                                             uint64_t serv,
                                             TagType tag_type,
                                             std::ostream& request_body) {
  auto tags = _serv_tag->get<0>().equal_range(host_serv_pair{host, serv});
  bool first = true;
  for (; tags.first != tags.second; ++tags.first) {
    id_to_tag::const_iterator tag_search = _id_to_tag->find(tags.first->tag);
    if (tag_search != _id_to_tag->end() &&
        tag_search->second.tag_type == tag_type) {
      if (first) {
        request_body << tag_search->second.name;
        first = false;
      } else {
        request_body << ',' << tag_search->second.name;
      }
    }
  }
}
