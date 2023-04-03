/*
** Copyright 2023 Centreon
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

#include "com/centreon/broker/cache/global_cache_data.hh"
#include "com/centreon/broker/log_v2.hh"

using namespace com::centreon::broker::cache;

void global_cache_data::managed_map(bool create) {
  global_cache::managed_map(create);
  _metric_info = _file->find_or_construct<id_to_metric_info>("metric_info")(
      _file->get_segment_manager());
  _metric_name = _file->find_or_construct<dictionnary>("metric_name")(
      _file->get_segment_manager());
  _metric_unit = _file->find_or_construct<dictionnary>("metric_unit")(
      _file->get_segment_manager());
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

const string& global_cache_data::get_from_dictionnary(
    dictionnary& dico,
    const absl::string_view& value) {
  auto exist =
      dico.find(value, string_string_view_hash(), string_string_view_equal());
  if (exist != dico.end()) {
    return *exist;
  }
  return *dico.emplace(value.data(), value.size(), _file->get_segment_manager())
              .first;
}

/***********************************************************/
/*                   feeders                               */
/***********************************************************/

void global_cache_data::set_metric_info(uint64_t metric_id,
                                        uint64_t index_id,
                                        const absl::string_view& name,
                                        const absl::string_view& unit,
                                        double min,
                                        double max) {
  try {
    absl::WriterMutexLock l(&_protect);
    _checksum_to_compute = true;
    auto exist = _metric_info->find(metric_id);
    if (exist != _metric_info->end()) {
      metric_info& to_update = exist->second;
      if (*to_update.name != name) {
        to_update.name = &get_metric_name(name);
      }
      if (*to_update.unit != unit) {
        to_update.unit = &get_metric_unit(unit);
      }
      to_update.min = min;
      to_update.max = max;
      to_update.index_id = index_id;
    } else {
      if (_metric_info->capacity() <= _metric_info->size()) {  // need to grow?
        _metric_info->reserve(_metric_info->capacity() + 0x10000);
      }

      metric_info to_insert = {.index_id = index_id,
                               .name = &get_metric_name(name),
                               .unit = &get_metric_unit(unit),
                               .min = min,
                               .max = max};
      _metric_info->emplace(metric_id, to_insert);
    }
  } catch (const interprocess::bad_alloc& e) {
    SPDLOG_LOGGER_DEBUG(log_v2::core(), "file full => grow");
    allocation_exception_handler();
    set_metric_info(metric_id, index_id, name, unit, min, max);
  }
}

void global_cache_data::store_instance(uint64_t instance_id,
                                       const absl::string_view& instance_name) {
  try {
    absl::WriterMutexLock l(&_protect);
    auto exist = _id_to_instance->find(instance_id);
    if (exist == _id_to_instance->end()) {
      _checksum_to_compute = true;

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
      _checksum_to_compute = true;
    }
  } catch (const interprocess::bad_alloc& e) {
    SPDLOG_LOGGER_DEBUG(log_v2::core(), "file full => grow");
    allocation_exception_handler();
    store_instance(instance_id, instance_name);
  }
}

void global_cache_data::store_host(uint64_t host_id,
                                   const absl::string_view& host_name,
                                   uint64_t resource_id,
                                   uint64_t severity_id) {
  try {
    absl::WriterMutexLock l(&_protect);
    _checksum_to_compute = true;
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
    SPDLOG_LOGGER_DEBUG(log_v2::core(), "file full => grow");
    allocation_exception_handler();
    store_host(host_id, host_name, resource_id, severity_id);
  }
}

void global_cache_data::store_service(
    uint64_t host_id,
    uint64_t service_id,
    const absl::string_view& service_description,
    uint64_t resource_id,
    uint64_t severity_id) {
  try {
    absl::WriterMutexLock l(&_protect);
    _checksum_to_compute = true;
    auto exist = _id_to_service->find({host_id, service_id});
    if (exist == _id_to_service->end()) {
      if (_id_to_service->size() ==
          _id_to_service->capacity()) {  // need to grow?
        _id_to_service->reserve(_id_to_service->capacity() + 0x1000);
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
    SPDLOG_LOGGER_DEBUG(log_v2::core(), "file full => grow");
    allocation_exception_handler();
    store_service(host_id, service_id, service_description, resource_id,
                  severity_id);
  }
}

void global_cache_data::set_index_mapping(uint64_t index_id,
                                          uint64_t host_id,
                                          uint64_t service_id) {
  try {
    absl::WriterMutexLock l(&_protect);
    if (_index_id_mapping->size() ==
        _index_id_mapping->capacity()) {  // need to grow
      _index_id_mapping->reserve(_index_id_mapping->size() + 0x10000);
      _checksum_to_compute = true;
    }
    if (_index_id_mapping
            ->emplace(index_id, host_serv_pair(host_id, service_id))
            .second) {
      _checksum_to_compute = true;
    }
  } catch (const interprocess::bad_alloc& e) {
    SPDLOG_LOGGER_DEBUG(log_v2::core(), "file full => grow");
    allocation_exception_handler();
    set_index_mapping(index_id, host_id, service_id);
  }
}

void global_cache_data::add_host_group(uint64_t group, uint64_t host) {
  try {
    absl::WriterMutexLock l(&_protect);
    if (_host_group->emplace(host_group_element{host, group}).second) {
      _checksum_to_compute = true;
    }
  } catch (const interprocess::bad_alloc& e) {
    SPDLOG_LOGGER_DEBUG(log_v2::core(), "file full => grow");
    allocation_exception_handler();
    add_host_group(group, host);
  }
}

void global_cache_data::remove_host_from_group(uint64_t group, uint64_t host) {
  absl::WriterMutexLock l(&_protect);
  _checksum_to_compute = true;
  _host_group->get<2>().erase(host_group_element{host, group});
}

void global_cache_data::remove_host_group(uint64_t group) {
  absl::WriterMutexLock l(&_protect);
  _checksum_to_compute = true;
  _host_group->get<1>().erase(group);
}

void global_cache_data::add_service_group(uint64_t group,
                                          uint64_t host,
                                          uint64_t service) {
  try {
    absl::WriterMutexLock l(&_protect);
    if (_service_group->emplace(service_group_element{{host, service}, group})
            .second) {
      _checksum_to_compute = true;
    }
  } catch (const interprocess::bad_alloc& e) {
    SPDLOG_LOGGER_DEBUG(log_v2::core(), "file full => grow");
    allocation_exception_handler();
    add_service_group(group, host, service);
  }
}

void global_cache_data::remove_service_from_group(uint64_t group,
                                                  uint64_t host,
                                                  uint64_t service) {
  absl::WriterMutexLock l(&_protect);
  _checksum_to_compute = true;
  _service_group->get<2>().erase(service_group_element{{host, service}, group});
}

void global_cache_data::remove_service_group(uint64_t group) {
  absl::WriterMutexLock l(&_protect);
  _checksum_to_compute = true;
  _service_group->get<1>().erase(group);
}

void global_cache_data::add_tag(uint64_t tag_id,
                                const absl::string_view& tag_name,
                                TagType tag_type,
                                uint64_t poller_id) {
  try {
    absl::WriterMutexLock l(&_protect);
    _checksum_to_compute = true;
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
    SPDLOG_LOGGER_DEBUG(log_v2::core(), "file full => grow");
    allocation_exception_handler();
    add_tag(tag_id, tag_name, tag_type, poller_id);
  }
}

void global_cache_data::remove_tag(uint64_t tag_id) {
  absl::WriterMutexLock l(&_protect);
  _id_to_tag->erase(tag_id);
  _host_tag->get<1>().erase(tag_id);
  _serv_tag->get<1>().erase(tag_id);
}

void global_cache_data::set_host_tag(uint64_t host,
                                     tag_id_enumerator&& tag_filler) {
  try {
    absl::WriterMutexLock l(&_protect);
    uint64_t tag_id;
    while (tag_id = tag_filler()) {
      _host_tag->insert({host, tag_id});
    }
  } catch (const interprocess::bad_alloc& e) {
    SPDLOG_LOGGER_DEBUG(log_v2::core(), "file full => grow");
    allocation_exception_handler();
    set_host_tag(host, std::move(tag_filler));
  }
}

void global_cache_data::set_serv_tag(uint64_t host,
                                     uint64_t serv,
                                     tag_id_enumerator&& tag_filler) {
  try {
    absl::WriterMutexLock l(&_protect);
    uint64_t tag_id;
    while (tag_id = tag_filler()) {
      _serv_tag->insert({{host, serv}, tag_id});
    }
  } catch (const interprocess::bad_alloc& e) {
    SPDLOG_LOGGER_DEBUG(log_v2::core(), "file full => grow");
    allocation_exception_handler();
    set_serv_tag(host, serv, std::move(tag_filler));
  }
}

/***********************************************************/
/*                   getters                               */
/***********************************************************/
const metric_info* global_cache_data::get_metric_info(
    uint32_t metric_id) const {
  auto search = _metric_info->find(metric_id);
  if (search != _metric_info->end()) {
    return &search->second;
  }
  return nullptr;
}

const string* global_cache_data::get_instance_name(uint64_t instance_id) const {
  auto search = _id_to_instance->find(instance_id);
  if (search != _id_to_instance->end()) {
    return &search->second;
  }
  return nullptr;
}

const resource_info* global_cache_data::get_host(uint64_t host_id) const {
  auto search = _id_to_host->find(host_id);
  if (search != _id_to_host->end()) {
    return &search->second;
  }
  return nullptr;
}

const resource_info* global_cache_data::get_host_from_index_id(
    uint64_t index_id) const {
  auto host_id_search = _index_id_mapping->find(index_id);
  if (host_id_search == _index_id_mapping->end()) {
    return nullptr;
  }
  return get_host(host_id_search->second.first);
}

const resource_info* global_cache_data::get_service(uint64_t host_id,
                                                    uint64_t service_id) const {
  auto serv_search = _id_to_service->find(host_serv_pair(host_id, service_id));
  if (serv_search != _id_to_service->end()) {
    return &serv_search->second;
  }
  return nullptr;
}

const resource_info* global_cache_data::get_service_from_index_id(
    uint64_t index_id) const {
  auto host_serv_id_search = _index_id_mapping->find(index_id);
  if (host_serv_id_search == _index_id_mapping->end()) {
    return nullptr;
  }
  return get_service(host_serv_id_search->second.first,
                     host_serv_id_search->second.second);
}

const host_serv_pair* global_cache_data::get_host_serv_id(
    uint64_t index_id) const {
  auto host_serv_id_search = _index_id_mapping->find(index_id);
  if (host_serv_id_search == _index_id_mapping->end()) {
    return nullptr;
  }
  return &host_serv_id_search->second;
}

void global_cache_data::append_service_group(uint64_t host,
                                             uint64_t service,
                                             std::ostream& request_body) {
  std::set<uint64_t> sorted;
  {
    absl::ReaderMutexLock l(&_protect);
    auto range =
        _service_group->get<0>().equal_range(host_serv_pair{host, service});
    if (range.first != range.second) {
      // in order to avoid cardinality, we sort results
      for (; range.first != range.second; ++range.first) {
        sorted.insert(range.first->group);
      }
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

void global_cache_data::append_host_group(uint64_t host,
                                          std::ostream& request_body) {
  std::set<uint64_t> sorted;
  {
    absl::ReaderMutexLock l(&_protect);
    auto range = _host_group->get<0>().equal_range(host);
    if (range.first != range.second) {
      // in order to avoid cardinality, we sort results
      for (; range.first != range.second; ++range.first) {
        sorted.insert(range.first->group);
      }
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
