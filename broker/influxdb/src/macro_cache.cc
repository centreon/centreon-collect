/**
 * Copyright 2011-2015, 2019-2024 Centreon
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

#include "com/centreon/broker/influxdb/macro_cache.hh"
#include "bbdo/storage/index_mapping.hh"
#include "bbdo/storage/metric_mapping.hh"

using namespace com::centreon::exceptions;
using namespace com::centreon::broker;
using namespace com::centreon::broker::influxdb;

/**
 *  Construct a macro cache
 *
 *  @param[in] cache  Persistent cache used by the macro cache.
 */
macro_cache::macro_cache(const std::shared_ptr<persistent_cache>& cache)
    : _cache(cache) {
  if (_cache != nullptr) {
    std::shared_ptr<io::data> d;
    do {
      _cache->get(d);
      write(d);
    } while (d);
  }
}

/**
 *  Destructor.
 */
macro_cache::~macro_cache() {
  if (_cache != nullptr) {
    try {
      _save_to_disk();
    } catch (std::exception const& e) {
      _cache->logger()->error(
          "influxdb: macro cache couldn't save data to disk: '{}'", e.what());
    }
  }
}

/**
 *  Get the mapping of an index.
 *
 *  @param[in] index_id   ID of the index.
 *
 *  @return               The status mapping.
 */
const storage::pb_index_mapping& macro_cache::get_index_mapping(
    uint64_t index_id) const {
  const auto found = _index_mappings.find(index_id);
  if (found == _index_mappings.end())
    throw msg_fmt("influxdb: could not find host/service of index {} ",
                  index_id);
  return *found->second;
}

/**
 *  Get the metric mapping of a metric.
 *
 *  @param[in] metric_id  The id of this metric.
 *
 *  @return               The metric mapping.
 */
const storage::pb_metric_mapping& macro_cache::get_metric_mapping(
    uint64_t metric_id) const {
  auto const found = _metric_mappings.find(metric_id);
  if (found == _metric_mappings.end())
    throw msg_fmt("influxdb: could not find index of metric {} ", metric_id);
  return *found->second;
}

/**
 *  Get the name of a host.
 *
 *  @param[in] host_id  The id of the host.
 *
 *  @return             The name of the host.
 */
std::string const& macro_cache::get_host_name(uint64_t host_id) const {
  auto const found = _hosts.find(host_id);
  if (found == _hosts.end())
    throw msg_fmt("influxdb: could not find information on host {} ", host_id);
  if (found->second->type() == neb::host::static_type()) {
    auto const& h = std::static_pointer_cast<neb::host>(found->second);
    return h->host_name;
  } else {
    auto const& h = std::static_pointer_cast<neb::pb_host>(found->second);
    return h->obj().name();
  }
}

/**
 *  Get the description of a service.
 *
 *  @param[in] host_id  The id of the host.
 *  @param service_id
 *
 *  @return             The description of the service.
 */
std::string const& macro_cache::get_service_description(
    uint64_t host_id,
    uint64_t service_id) const {
  auto const found = _services.find({host_id, service_id});
  if (found == _services.end())
    throw msg_fmt("influxdb: could not find information on service ({}, {})",
                  host_id, service_id);
  if (found->second->type() == neb::service::static_type()) {
    auto const& s = std::static_pointer_cast<neb::service>(found->second);
    return s->service_description;
  } else {
    auto const& s = std::static_pointer_cast<neb::pb_service>(found->second);
    return s->obj().description();
  }
}

/**
 *  Get the name of an instance.
 *
 *  @param[in] instance_id  The id of the instance.
 *
 *  @return   The name of the instance.
 */
std::string const& macro_cache::get_instance(uint64_t instance_id) const {
  auto const found = _instances.find(instance_id);
  if (found == _instances.end())
    throw msg_fmt("influxdb: could not find information on instance {}",
                  instance_id);
  return found->second->type() == neb::instance::static_type()
             ? std::static_pointer_cast<neb::instance>(found->second)->name
             : std::static_pointer_cast<neb::pb_instance>(found->second)
                   ->obj()
                   .name();
}

/**
 *  Write an event into the cache.
 *
 *  @param[in] data  The event to write.
 */
void macro_cache::write(const std::shared_ptr<io::data>& data) {
  if (!data)
    return;

  switch (data->type()) {
    case neb::instance::static_type():
      _process_instance(data);
      break;
    case neb::pb_instance::static_type():
      _process_pb_instance(data);
      break;
    case neb::host::static_type():
      _process_host(data);
      break;
    case neb::pb_host::static_type():
      _process_pb_host(data);
      break;
    case neb::service::static_type():
      _process_service(data);
      break;
    case neb::pb_service::static_type():
      _process_pb_service(data);
      break;
    case storage::pb_index_mapping::static_type():
      _process_index_mapping(data);
      break;
    case storage::index_mapping::static_type():
      _process_index_mapping(data);
      break;
    case storage::pb_metric_mapping::static_type():
      _process_metric_mapping(data);
      break;
    case storage::metric_mapping::static_type():
      _process_metric_mapping(data);
      break;
    default:
      break;
  }
}

/**
 *  Process an instance event.
 *
 *  @param data  The event.
 */
void macro_cache::_process_instance(std::shared_ptr<io::data> const& data) {
  auto const& in = std::static_pointer_cast<neb::instance>(data);
  _instances[in->poller_id] = in;
}

/**
 *  Process an instance event.
 *
 *  @param data  The event.
 */
void macro_cache::_process_pb_instance(std::shared_ptr<io::data> const& data) {
  auto const& in = std::static_pointer_cast<neb::pb_instance>(data);
  _instances[in->obj().instance_id()] = in;
}

/**
 *  Process a host event.
 *
 *  @param data  The event.
 */
void macro_cache::_process_host(std::shared_ptr<io::data> const& data) {
  auto const& h = std::static_pointer_cast<neb::host>(data);
  _hosts[h->host_id] = data;
}

/**
 *  Process a host event.
 *
 *  @param data  The event.
 */
void macro_cache::_process_pb_host(std::shared_ptr<io::data> const& data) {
  auto const& h = std::static_pointer_cast<neb::pb_host>(data);
  _hosts[h->obj().host_id()] = data;
}

/**
 *  Process a service event.
 *
 *  @param data  The event.
 */
void macro_cache::_process_service(std::shared_ptr<io::data> const& data) {
  auto const& s = std::static_pointer_cast<neb::service>(data);
  _services[{s->host_id, s->service_id}] = data;
}

/**
 *  Process a pb service event.
 *
 *  @param data  The event.
 */
void macro_cache::_process_pb_service(std::shared_ptr<io::data> const& data) {
  auto const& s = std::static_pointer_cast<neb::pb_service>(data);
  _services[{s->obj().host_id(), s->obj().service_id()}] = data;
}

/**
 *  Process an index mapping event.
 *
 *  @param im  The event.
 */
void macro_cache::_process_index_mapping(
    std::shared_ptr<io::data> const& data) {
  switch (data->type()) {
    case storage::pb_index_mapping::static_type(): {
      auto im = std::static_pointer_cast<storage::pb_index_mapping>(data);
      _index_mappings[im->obj().index_id()] = im;
    } break;
    case storage::index_mapping::static_type(): {
      const auto& im = std::static_pointer_cast<storage::index_mapping>(data);
      auto pb_im = std::make_shared<storage::pb_index_mapping>();
      auto& obj = pb_im->mut_obj();
      obj.set_index_id(im->index_id);
      obj.set_host_id(im->host_id);
      obj.set_service_id(im->service_id);
      _index_mappings[obj.index_id()] = pb_im;
    } break;
    default:
      /* Should not arrive */
      assert(1 == 0);
      break;
  }
}

/**
 *  Process a metric mapping event.
 *
 *  @param mm  The event.
 */
void macro_cache::_process_metric_mapping(
    std::shared_ptr<io::data> const& data) {
  switch (data->type()) {
    case storage::pb_metric_mapping::static_type(): {
      const auto& mm =
          std::static_pointer_cast<storage::pb_metric_mapping>(data);
      _metric_mappings[mm->obj().metric_id()] = mm;
    } break;
    case storage::metric_mapping::static_type(): {
      /* To avoid conflicts and thinking about future we force all the cache
       * to contain protobuf messages. */
      const auto& mm = std::static_pointer_cast<storage::metric_mapping>(data);
      auto pb_mm = std::make_shared<storage::pb_metric_mapping>();
      auto& obj = pb_mm->mut_obj();
      obj.set_index_id(mm->index_id);
      obj.set_metric_id(mm->metric_id);
      _metric_mappings[obj.metric_id()] = pb_mm;
    } break;
    default:
      /* Should not arrive */
      assert(1 == 0);
      break;
  }
}

/**
 *  Save all data to disk.
 */
void macro_cache::_save_to_disk() {
  _cache->transaction();

  for (auto it = _instances.begin(), end = _instances.end(); it != end; ++it)
    _cache->add(it->second);

  for (auto it = _hosts.begin(), end = _hosts.end(); it != end; ++it)
    _cache->add(it->second);

  for (auto it = _services.begin(), end = _services.end(); it != end; ++it)
    _cache->add(it->second);

  for (auto it = _index_mappings.begin(), end = _index_mappings.end();
       it != end; ++it)
    _cache->add(it->second);

  for (auto it = _metric_mappings.begin(), end = _metric_mappings.end();
       it != end; ++it)
    _cache->add(it->second);

  _cache->commit();
}
