/*
** Copyright 2017-2022 Centreon
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

#include "com/centreon/broker/lua/macro_cache.hh"
#include "bbdo/bam/dimension_ba_bv_relation_event.hh"
#include "bbdo/bam/dimension_ba_event.hh"
#include "bbdo/bam/dimension_bv_event.hh"
#include "bbdo/storage/index_mapping.hh"
#include "bbdo/storage/metric_mapping.hh"
#include "com/centreon/exceptions/msg_fmt.hh"
#include "common/log_v2/log_v2.hh"

using namespace com::centreon::exceptions;
using namespace com::centreon::broker;
using namespace com::centreon::broker::lua;
using com::centreon::common::log_v2::log_v2;

/**
 *  Construct a macro cache
 *
 *  @param[in] cache  Persistent cache used by the macro cache.
 */
macro_cache::macro_cache(const std::shared_ptr<persistent_cache>& cache,
                         const uint32_t logger_id)
    : _cache(cache) {
  if (_cache != nullptr) {
    _cache->set_logger_id(logger_id);
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
      SPDLOG_LOGGER_ERROR(_cache->logger(),
                          "lua: macro cache couldn't save data to disk: '{}'",
                          e.what());
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
    throw msg_fmt("lua: could not find host/service of index {}", index_id);
  return *found->second;
}

/**
 *  Get the metric mapping of a metric.
 *
 *  @param[in] metric_id  The id of this metric.
 *
 *  @return               The metric mapping.
 */
const std::shared_ptr<storage::pb_metric_mapping>&
macro_cache::get_metric_mapping(uint64_t metric_id) const {
  auto const found = _metric_mappings.find(metric_id);
  if (found == _metric_mappings.end())
    throw msg_fmt("lua: could not find index of metric {}", metric_id);
  return found->second;
}

/**
 *  Get the full service.
 *
 *  @param[in] host_id  The id of the host.
 *  @param[in] service_id  The id of the service.
 *
 *  @return             A shared pointer on the service.
 */
const std::shared_ptr<io::data>& macro_cache::get_service(
    uint64_t host_id,
    uint64_t service_id) const {
  auto found = _services.find({host_id, service_id});

  if (found == _services.end())
    throw msg_fmt("lua: could not find information on service ({}, {})",
                  host_id, service_id);
  return found->second;
}

/**
 *  Get the full host.
 *
 *  @param[in] host_id  The id of the host.
 *
 *  @return             A shared pointer on the host.
 */
const std::shared_ptr<io::data>& macro_cache::get_host(uint64_t host_id) const {
  auto found = _hosts.find(host_id);

  if (found == _hosts.end())
    throw msg_fmt("lua: could not find information on host {}", host_id);

  return found->second;
}

/**
 *  Get the name of a host.
 *
 *  @param[in] host_id  The id of the host.
 *
 *  @return             The name of the host.
 */
std::string const& macro_cache::get_host_name(uint64_t host_id) const {
  auto found = _hosts.find(host_id);

  if (found == _hosts.end())
    throw msg_fmt("lua: could not find information on host {}", host_id);

  if (found->second->type() == neb::host::static_type()) {
    auto const& s = std::static_pointer_cast<neb::host>(found->second);
    return s->host_name;
  } else {
    auto const& s = std::static_pointer_cast<neb::pb_host>(found->second);
    return s->obj().name();
  }
}

/**
 * Get the severity about a service or a host.
 *
 * @param host_id
 * @param service_id The service id or 0 in case of a host.
 *
 * @return a severity (int32_t).
 */
int32_t macro_cache::get_severity(uint64_t host_id, uint64_t service_id) const {
  auto found = _custom_vars.find({host_id, service_id});
  if (found == _custom_vars.end())
    throw msg_fmt(
        "lua: could not find the severity of the object (host_id: {}, "
        "service_id: {})",
        host_id, service_id);
  int32_t ret;
  if (found->second->type() == neb::custom_variable::static_type()) {
    if (absl::SimpleAtoi(
            std::static_pointer_cast<neb::custom_variable>(found->second)
                ->value,
            &ret)) {
      return ret;
    } else {
      return 0;
    }
  } else {
    if (absl::SimpleAtoi(
            std::static_pointer_cast<neb::pb_custom_variable>(found->second)
                ->obj()
                .value(),
            &ret)) {
      return ret;
    } else {
      return 0;
    }
  }
}

/**
 * @brief Get the resource check command from the given host_id, service_id. If
 * only the host_id is given then the service_id is considered to be 0 and we
 * looke for a host check command. Otherwise we looke for a service check
 * command.
 *
 * @param host_id An integer representing a host ID.
 * @param service_id An service ID or 0 for a host.
 *
 * @return A string view pointing to the check command.
 */
absl::string_view macro_cache::get_check_command(uint64_t host_id,
                                                 uint64_t service_id) const {
  /* Case of services */
  absl::string_view retval;
  if (service_id) {
    auto found = _services.find({host_id, service_id});
    if (found == _services.end())
      throw msg_fmt(
          "lua: could not find the check command of the service (host_id: {}, "
          "service_id: {})",
          host_id, service_id);
    if (found->second->type() == neb::service::static_type()) {
      neb::service& s = static_cast<neb::service&>(*found->second);
      retval = s.check_command;
    } else {
      neb::pb_service& s = static_cast<neb::pb_service&>(*found->second);
      retval = s.obj().check_command();
    }
  }
  /* Case of hosts */
  else {
    auto found = _hosts.find(host_id);
    if (found == _hosts.end())
      throw msg_fmt(
          "lua: could not find the check command of the host (host_id: {})",
          host_id);
    if (found->second->type() == neb::host::static_type()) {
      neb::host& s = static_cast<neb::host&>(*found->second);
      retval = s.check_command;
    } else {
      neb::pb_host& s = static_cast<neb::pb_host&>(*found->second);
      retval = s.obj().check_command();
    }
  }
  return retval;
}

/**
 *  Get the notes url of a host (service_id=0) or a service.
 *
 *  @param[in] host_id     The id of the host.
 *  @param[in] service_id  The id of the service.
 *
 *  @return             The notes url.
 */
std::string const& macro_cache::get_notes_url(uint64_t host_id,
                                              uint64_t service_id) const {
  if (service_id) {
    auto found = _services.find({host_id, service_id});

    if (found == _services.end())
      throw msg_fmt("lua: could not find information on service ({}, {})",
                    host_id, service_id);
    if (found->second->type() == neb::service::static_type()) {
      auto const& s = std::static_pointer_cast<neb::service>(found->second);
      return s->notes_url;
    } else {
      auto const& s = std::static_pointer_cast<neb::pb_service>(found->second);
      return s->obj().notes_url();
    }
  } else {
    auto found = _hosts.find(host_id);

    if (found == _hosts.end())
      throw msg_fmt("lua: could not find information on host {}", host_id);

    if (found->second->type() == neb::host::static_type()) {
      auto const& s = std::static_pointer_cast<neb::host>(found->second);
      return s->notes_url;
    } else {
      auto const& s = std::static_pointer_cast<neb::pb_host>(found->second);
      return s->obj().notes_url();
    }
  }
}

/**
 *  Get the action url of a host (service_id=0) or a service.
 *
 *  @param[in] host_id  The id of the host.
 *  @param[in] host_id  The id of the service.
 *
 *  @return             The action url.
 */
std::string const& macro_cache::get_action_url(uint64_t host_id,
                                               uint64_t service_id) const {
  if (service_id) {
    auto found = _services.find({host_id, service_id});

    if (found == _services.end())
      throw msg_fmt("lua: could not find information on service ({}, {})",
                    host_id, service_id);
    if (found->second->type() == neb::service::static_type()) {
      auto const& s = std::static_pointer_cast<neb::service>(found->second);
      return s->action_url;
    } else {
      auto const& s = std::static_pointer_cast<neb::pb_service>(found->second);
      return s->obj().action_url();
    }
  } else {
    auto found = _hosts.find(host_id);

    if (found == _hosts.end())
      throw msg_fmt("lua: could not find information on host {}", host_id);

    if (found->second->type() == neb::host::static_type()) {
      auto const& s = std::static_pointer_cast<neb::host>(found->second);
      return s->action_url;
    } else {
      auto const& s = std::static_pointer_cast<neb::pb_host>(found->second);
      return s->obj().action_url();
    }
  }
}

/**
 *  Get the notes of a host (service_id=0) or a service.
 *
 *  @param[in] host_id     The id of the host.
 *  @param[in] service_id  The id of the service.
 *
 *  @return             The notes.
 */
std::string const& macro_cache::get_notes(uint64_t host_id,
                                          uint64_t service_id) const {
  if (service_id) {
    auto found = _services.find({host_id, service_id});

    if (found == _services.end())
      throw msg_fmt("lua: cound not find information on service ({}, {})",
                    host_id, service_id);
    if (found->second->type() == neb::service::static_type()) {
      auto const& s = std::static_pointer_cast<neb::service>(found->second);
      return s->notes;
    } else {
      auto const& s = std::static_pointer_cast<neb::pb_service>(found->second);
      return s->obj().notes();
    }
  } else {
    auto found = _hosts.find(host_id);

    if (found == _hosts.end())
      throw msg_fmt("lua: could not find information on host {}", host_id);

    if (found->second->type() == neb::host::static_type()) {
      auto const& s = std::static_pointer_cast<neb::host>(found->second);
      return s->notes;
    } else {
      auto const& s = std::static_pointer_cast<neb::pb_host>(found->second);
      return s->obj().notes();
    }
  }
}

/**
 *  Get a map of the host groups members index by host_id and host_group.
 *
 *  @return             A std::map
 */
const absl::btree_map<std::pair<uint64_t, uint64_t>,
                      std::shared_ptr<neb::host_group_member>>&
macro_cache::get_host_group_members() const {
  return _host_group_members;
}

/**
 *  Get the name of a host group.
 *
 *  @param[in] id  The id of the host group.
 *
 *  @return             The name of the host group.
 */
std::string const& macro_cache::get_host_group_name(uint64_t id) const {
  auto const found = _host_groups.find(id);

  if (found == _host_groups.end())
    throw msg_fmt("lua: could not find information on host group {}", id);
  return found->second->name;
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
    throw msg_fmt("lua: could not find information on service ({}, {})",
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
 *  Service group members accessor
 *
 *  @param[in] host_id  The id of the host.
 *  @param[in] service_id  The id of the service.
 *
 *  @return   A map indexed by host_id/service_id/group_id.
 */
absl::btree_map<std::tuple<uint64_t, uint64_t, uint64_t>,
                std::shared_ptr<neb::service_group_member>> const&
macro_cache::get_service_group_members() const {
  return _service_group_members;
}

/**
 *  Get the name of a service group.
 *
 *  @param[in] id  The id of the service group.
 *
 *  @return            The name of the service group.
 */
std::string const& macro_cache::get_service_group_name(uint64_t id) const {
  auto found = _service_groups.find(id);

  if (found == _service_groups.end())
    throw msg_fmt("lua: could not find information on service group {}", id);
  return found->second->name;
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
    throw msg_fmt("lua: could not find information on instance {}",
                  instance_id);
  return found->second->type() == neb::instance::static_type()
             ? std::static_pointer_cast<neb::instance>(found->second)->name
             : std::static_pointer_cast<neb::pb_instance>(found->second)
                   ->obj()
                   .name();
}

/**
 *  Accessor to the multi hash containing ba bv relations.
 *
 * @return A reference to an unordered_multimap containining ba/bv relation
 * events.
 */
std::unordered_multimap<
    uint64_t,
    std::shared_ptr<bam::pb_dimension_ba_bv_relation_event>> const&
macro_cache::get_dimension_ba_bv_relation_events() const {
  return _dimension_ba_bv_relation_events;
}

/**
 *  Return a dimension_ba_event from its id.
 *
 * @param ba_id The id
 *
 * @return a reference to the dimension_ba_event.
 */
const std::shared_ptr<bam::pb_dimension_ba_event>&
macro_cache::get_dimension_ba_event(uint64_t ba_id) const {
  auto const found = _dimension_ba_events.find(ba_id);
  if (found == _dimension_ba_events.end())
    throw msg_fmt("lua: could not find information on dimension ba event {}",
                  ba_id);
  return found->second;
}

/**
 *  Return a dimension_bv_event from its id.
 *
 * @param bv_id The id
 *
 * @return a reference to the dimension_bv_event.
 */
const std::shared_ptr<bam::pb_dimension_bv_event>&
macro_cache::get_dimension_bv_event(uint64_t bv_id) const {
  auto const found = _dimension_bv_events.find(bv_id);
  if (found == _dimension_bv_events.end())
    throw msg_fmt("lua: could not find information on dimension bv event {}",
                  bv_id);
  return found->second;
}

/**
 *  Write an event into the cache.
 *
 *  @param[in] data  The event to write.
 */
void macro_cache::write(std::shared_ptr<io::data> const& data) {
  _cache->update_logger();
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
    case neb::pb_adaptive_host::static_type():
      _process_pb_adaptive_host(data);
      break;
    case neb::host_group::static_type():
      _process_host_group(data);
      break;
    case neb::host_group_member::static_type():
      _process_host_group_member(data);
      break;
    case neb::service::static_type():
      _process_service(data);
      break;
    case neb::pb_service::static_type():
      _process_pb_service(data);
      break;
    case neb::pb_adaptive_service::static_type():
      _process_pb_adaptive_service(data);
      break;
    case neb::service_group::static_type():
      _process_service_group(data);
      break;
    case neb::service_group_member::static_type():
      _process_service_group_member(data);
      break;
    case neb::custom_variable::static_type():
      _process_custom_variable(data);
      break;
    case neb::pb_custom_variable::static_type():
      _process_pb_custom_variable(data);
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
    case bam::dimension_ba_event::static_type():
    case bam::pb_dimension_ba_event::static_type():
      _process_dimension_ba_event(data);
      break;
    case bam::dimension_ba_bv_relation_event::static_type():
    case bam::pb_dimension_ba_bv_relation_event::static_type():
      _process_dimension_ba_bv_relation_event(data);
      break;
    case bam::dimension_bv_event::static_type():
    case bam::pb_dimension_bv_event::static_type():
      _process_dimension_bv_event(data);
      break;
    case bam::dimension_truncate_table_signal::static_type():
      _process_dimension_truncate_table_signal(data);
      break;
    case bam::pb_dimension_truncate_table_signal::static_type():
      _process_pb_dimension_truncate_table_signal(data);
      break;
    default:
      break;
  }
}

/**
 *  Process an instance event.
 *
 *  @param in  The event.
 */
void macro_cache::_process_instance(std::shared_ptr<io::data> const& data) {
  std::shared_ptr<neb::instance> const& in =
      std::static_pointer_cast<neb::instance>(data);
  _instances[in->poller_id] = in;
}

/**
 *  Process an instance event.
 *
 *  @param in  The event.
 */
void macro_cache::_process_pb_instance(std::shared_ptr<io::data> const& data) {
  auto const& in = std::static_pointer_cast<neb::pb_instance>(data);
  _instances[in->obj().instance_id()] = in;
}

/**
 *  Process a host event.
 *
 *  @param h  The event.
 */
void macro_cache::_process_host(std::shared_ptr<io::data> const& data) {
  std::shared_ptr<neb::host> const& h =
      std::static_pointer_cast<neb::host>(data);
  SPDLOG_LOGGER_DEBUG(_cache->logger(), "lua: processing host '{}' of id {}",
                      h->host_name, h->host_id);
  if (h->enabled)
    _hosts[h->host_id] = data;
  else
    _hosts.erase(h->host_id);
}

/**
 *  Process a pb host event.
 *
 *  @param h  The event.
 */
void macro_cache::_process_pb_host(std::shared_ptr<io::data> const& data) {
  std::shared_ptr<neb::pb_host> const& h =
      std::static_pointer_cast<neb::pb_host>(data);
  SPDLOG_LOGGER_DEBUG(_cache->logger(), "lua: processing host '{}' of id {}",
                      h->obj().name(), h->obj().host_id());
  if (h->obj().enabled())
    _hosts[h->obj().host_id()] = data;
  else
    _hosts.erase(h->obj().host_id());
}

/**
 *  Process a pb adaptive host event.
 *
 *  @param s  The event.
 */
void macro_cache::_process_pb_adaptive_host(
    const std::shared_ptr<io::data>& data) {
  const auto& h = std::static_pointer_cast<neb::pb_adaptive_host>(data);
  SPDLOG_LOGGER_DEBUG(_cache->logger(), "lua: processing adaptive host {}",
                      h->obj().host_id());
  auto& ah = h->obj();
  auto it = _hosts.find(ah.host_id());
  if (it != _hosts.end()) {
    if (it->second->type() == make_type(io::neb, neb::de_host)) {
      auto& h = *std::static_pointer_cast<neb::host>(it->second);
      if (ah.has_notify())
        h.notifications_enabled = ah.notify();
      if (ah.has_active_checks())
        h.active_checks_enabled = ah.active_checks();
      if (ah.has_should_be_scheduled())
        h.should_be_scheduled = ah.should_be_scheduled();
      if (ah.has_passive_checks())
        h.passive_checks_enabled = ah.passive_checks();
      if (ah.has_event_handler_enabled())
        h.event_handler_enabled = ah.event_handler_enabled();
      if (ah.has_flap_detection())
        h.flap_detection_enabled = ah.flap_detection();
      if (ah.has_obsess_over_host())
        h.obsess_over = ah.obsess_over_host();
      if (ah.has_event_handler())
        h.event_handler = ah.event_handler();
      if (ah.has_check_command())
        h.check_command = ah.check_command();
      if (ah.has_check_interval())
        h.check_interval = ah.check_interval();
      if (ah.has_retry_interval())
        h.retry_interval = ah.retry_interval();
      if (ah.has_max_check_attempts())
        h.max_check_attempts = ah.max_check_attempts();
      if (ah.has_check_freshness())
        h.check_freshness = ah.check_freshness();
      if (ah.has_check_period())
        h.check_period = ah.check_period();
      if (ah.has_notification_period())
        h.notification_period = ah.notification_period();
    } else {
      auto& h = std::static_pointer_cast<neb::pb_host>(it->second)->mut_obj();
      if (ah.has_notify())
        h.set_notify(ah.notify());
      if (ah.has_active_checks())
        h.set_active_checks(ah.active_checks());
      if (ah.has_should_be_scheduled())
        h.set_should_be_scheduled(ah.should_be_scheduled());
      if (ah.has_passive_checks())
        h.set_passive_checks(ah.passive_checks());
      if (ah.has_event_handler_enabled())
        h.set_event_handler_enabled(ah.event_handler_enabled());
      if (ah.has_flap_detection())
        h.set_flap_detection(ah.flap_detection());
      if (ah.has_obsess_over_host())
        h.set_obsess_over_host(ah.obsess_over_host());
      if (ah.has_event_handler())
        h.set_event_handler(ah.event_handler());
      if (ah.has_check_command())
        h.set_check_command(ah.check_command());
      if (ah.has_check_interval())
        h.set_check_interval(ah.check_interval());
      if (ah.has_retry_interval())
        h.set_retry_interval(ah.retry_interval());
      if (ah.has_max_check_attempts())
        h.set_max_check_attempts(ah.max_check_attempts());
      if (ah.has_check_freshness())
        h.set_check_freshness(ah.check_freshness());
      if (ah.has_check_period())
        h.set_check_period(ah.check_period());
      if (ah.has_notification_period())
        h.set_notification_period(ah.notification_period());
    }
  } else
    SPDLOG_LOGGER_WARN(
        _cache->logger(),
        "lua: cannot update cache for host {}, it does not exist in "
        "the cache",
        h->obj().host_id());
}

/**
 *  Process a host group event.
 *
 *  @param data  The event.
 */
void macro_cache::_process_host_group(std::shared_ptr<io::data> const& data) {
  std::shared_ptr<neb::host_group> const& hg =
      std::static_pointer_cast<neb::host_group>(data);
  SPDLOG_LOGGER_DEBUG(_cache->logger(),
                      "lua: processing host group '{}' of id {}", hg->name,
                      hg->id);
  if (hg->enabled)
    _host_groups[hg->id] = hg;
}

/**
 *  Process a host group member event.
 *
 *  @param data  The event.
 */
void macro_cache::_process_host_group_member(
    std::shared_ptr<io::data> const& data) {
  std::shared_ptr<neb::host_group_member> const& hgm =
      std::static_pointer_cast<neb::host_group_member>(data);
  SPDLOG_LOGGER_DEBUG(
      _cache->logger(),
      "lua: processing host group member (group_name: '{}', group_id: {}, "
      "host_id: {})",
      hgm->group_name, hgm->group_id, hgm->host_id);
  if (hgm->enabled)
    _host_group_members[{hgm->host_id, hgm->group_id}] = hgm;
  else
    _host_group_members.erase({hgm->host_id, hgm->group_id});
}

/**
 *  Process a service event.
 *
 *  @param s  The event.
 */
void macro_cache::_process_service(std::shared_ptr<io::data> const& data) {
  auto const& s = std::static_pointer_cast<neb::service>(data);
  SPDLOG_LOGGER_DEBUG(_cache->logger(),
                      "lua: processing service ({}, {}) (description:{})",
                      s->host_id, s->service_id, s->service_description);
  if (s->enabled)
    _services[{s->host_id, s->service_id}] = data;
  else
    _services.erase({s->host_id, s->service_id});
}

/**
 *  Process a pb service event.
 *
 *  @param s  The event.
 */
void macro_cache::_process_pb_service(std::shared_ptr<io::data> const& data) {
  auto const& s = std::static_pointer_cast<neb::pb_service>(data);
  SPDLOG_LOGGER_DEBUG(
      _cache->logger(), "lua: processing service ({}, {}) (description:{})",
      s->obj().host_id(), s->obj().service_id(), s->obj().description());
  if (s->obj().enabled())
    _services[{s->obj().host_id(), s->obj().service_id()}] = data;
  else
    _services.erase({s->obj().host_id(), s->obj().service_id()});
}

/**
 *  Process a pb service event.
 *
 *  @param s  The event.
 */
void macro_cache::_process_pb_adaptive_service(
    std::shared_ptr<io::data> const& data) {
  const auto& s = std::static_pointer_cast<neb::pb_adaptive_service>(data);
  SPDLOG_LOGGER_DEBUG(_cache->logger(),
                      "lua: processing adaptive service ({}, {})",
                      s->obj().host_id(), s->obj().service_id());
  auto& as = s->obj();
  auto it = _services.find({as.host_id(), as.service_id()});
  if (it != _services.end()) {
    if (it->second->type() == make_type(io::neb, neb::de_service)) {
      auto& s = *std::static_pointer_cast<neb::service>(it->second);
      if (as.has_notify())
        s.notifications_enabled = as.notify();
      if (as.has_active_checks())
        s.active_checks_enabled = as.active_checks();
      if (as.has_should_be_scheduled())
        s.should_be_scheduled = as.should_be_scheduled();
      if (as.has_passive_checks())
        s.passive_checks_enabled = as.passive_checks();
      if (as.has_event_handler_enabled())
        s.event_handler_enabled = as.event_handler_enabled();
      if (as.has_flap_detection_enabled())
        s.flap_detection_enabled = as.flap_detection_enabled();
      if (as.has_obsess_over_service())
        s.obsess_over = as.obsess_over_service();
      if (as.has_event_handler())
        s.event_handler = as.event_handler();
      if (as.has_check_command())
        s.check_command = as.check_command();
      if (as.has_check_interval())
        s.check_interval = as.check_interval();
      if (as.has_retry_interval())
        s.retry_interval = as.retry_interval();
      if (as.has_max_check_attempts())
        s.max_check_attempts = as.max_check_attempts();
      if (as.has_check_freshness())
        s.check_freshness = as.check_freshness();
      if (as.has_check_period())
        s.check_period = as.check_period();
      if (as.has_notification_period())
        s.notification_period = as.notification_period();
    } else {
      auto& s =
          std::static_pointer_cast<neb::pb_service>(it->second)->mut_obj();
      if (as.has_notify())
        s.set_notify(as.notify());
      if (as.has_active_checks())
        s.set_active_checks(as.active_checks());
      if (as.has_should_be_scheduled())
        s.set_should_be_scheduled(as.should_be_scheduled());
      if (as.has_passive_checks())
        s.set_passive_checks(as.passive_checks());
      if (as.has_event_handler_enabled())
        s.set_event_handler_enabled(as.event_handler_enabled());
      if (as.has_flap_detection_enabled())
        s.set_flap_detection(as.flap_detection_enabled());
      if (as.has_obsess_over_service())
        s.set_obsess_over_service(as.obsess_over_service());
      if (as.has_event_handler())
        s.set_event_handler(as.event_handler());
      if (as.has_check_command())
        s.set_check_command(as.check_command());
      if (as.has_check_interval())
        s.set_check_interval(as.check_interval());
      if (as.has_retry_interval())
        s.set_retry_interval(as.retry_interval());
      if (as.has_max_check_attempts())
        s.set_max_check_attempts(as.max_check_attempts());
      if (as.has_check_freshness())
        s.set_check_freshness(as.check_freshness());
      if (as.has_check_period())
        s.set_check_period(as.check_period());
      if (as.has_notification_period())
        s.set_notification_period(as.notification_period());
    }
  } else {
    SPDLOG_LOGGER_WARN(
        _cache->logger(),
        "lua: cannot update cache for service ({}, {}), it does not exist in "
        "the cache",
        s->obj().host_id(), s->obj().service_id());
  }
}

/**
 *  Process a service group event.
 *
 *  @param sg  The event.
 */
void macro_cache::_process_service_group(
    std::shared_ptr<io::data> const& data) {
  auto const& sg = std::static_pointer_cast<neb::service_group>(data);
  SPDLOG_LOGGER_DEBUG(_cache->logger(),
                      "lua: processing service group '{}' of id {}", sg->name,
                      sg->id);
  if (sg->enabled)
    _service_groups[sg->id] = sg;
}

/**
 *  Process a service group member event.
 *
 *  @param data  The event.
 */
void macro_cache::_process_service_group_member(
    std::shared_ptr<io::data> const& data) {
  auto const& sgm = std::static_pointer_cast<neb::service_group_member>(data);
  SPDLOG_LOGGER_DEBUG(
      _cache->logger(),
      "lua: processing service group member (group_name: {}, group_id: {}, "
      "host_id: {}, service_id: {}",
      sgm->group_name, sgm->group_id, sgm->host_id, sgm->service_id);
  if (sgm->enabled)
    _service_group_members[std::make_tuple(sgm->host_id, sgm->service_id,
                                           sgm->group_id)] = sgm;
  else
    _service_group_members.erase(
        std::make_tuple(sgm->host_id, sgm->service_id, sgm->group_id));
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
 *  @param data  The event.
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
      _metric_mappings[obj.metric_id()] = std::move(pb_mm);
    } break;
    default:
      /* Should not arrive */
      assert(1 == 0);
      break;
  }
}

/**
 *  Process a dimension ba event
 *
 *  @param data  The event.
 */
void macro_cache::_process_dimension_ba_event(
    std::shared_ptr<io::data> const& data) {
  if (data->type() == bam::pb_dimension_ba_event::static_type()) {
    auto const& dbae =
        std::static_pointer_cast<bam::pb_dimension_ba_event>(data);
    SPDLOG_LOGGER_DEBUG(_cache->logger(),
                        "lua: pb processing dimension ba event of id {}",
                        dbae->obj().ba_id());
    _dimension_ba_events[dbae->obj().ba_id()] = dbae;
  } else {
    const auto& to_convert =
        std::static_pointer_cast<bam::dimension_ba_event>(data);
    auto dbae = std::make_shared<bam::pb_dimension_ba_event>();
    DimensionBaEvent& to_fill = dbae->mut_obj();
    to_fill.set_ba_id(to_convert->ba_id);
    to_fill.set_ba_name(to_convert->ba_name);
    to_fill.set_ba_description(to_convert->ba_description);
    to_fill.set_sla_month_percent_crit(to_convert->sla_month_percent_crit);
    to_fill.set_sla_month_percent_warn(to_convert->sla_month_percent_warn);
    to_fill.set_sla_duration_crit(to_convert->sla_duration_crit);
    to_fill.set_sla_duration_warn(to_convert->sla_duration_warn);
    SPDLOG_LOGGER_DEBUG(_cache->logger(),
                        "lua: pb processing dimension ba event of id {}",
                        dbae->obj().ba_id());
    _dimension_ba_events[dbae->obj().ba_id()] = dbae;
  }
}

/**
 *  Process a dimension ba bv relation event
 *
 *  @param data  The event.
 */
void macro_cache::_process_dimension_ba_bv_relation_event(
    std::shared_ptr<io::data> const& data) {
  if (data->type() == bam::pb_dimension_ba_bv_relation_event::static_type()) {
    const auto& pb_data =
        std::static_pointer_cast<bam::pb_dimension_ba_bv_relation_event>(data);
    const DimensionBaBvRelationEvent& rel = pb_data->obj();
    SPDLOG_LOGGER_DEBUG(_cache->logger(),
                        "lua: processing pb dimension ba bv relation event "
                        "(ba_id: {}, bv_id: {})",
                        rel.ba_id(), rel.bv_id());
    _dimension_ba_bv_relation_events.insert({rel.ba_id(), pb_data});
  } else {
    auto const& rel =
        std::static_pointer_cast<bam::dimension_ba_bv_relation_event>(data);
    SPDLOG_LOGGER_DEBUG(
        _cache->logger(),
        "lua: processing dimension ba bv relation event (ba_id: {}, bv_id: {})",
        rel->ba_id, rel->bv_id);
    auto pb_data(std::make_shared<bam::pb_dimension_ba_bv_relation_event>());
    pb_data->mut_obj().set_ba_id(rel->ba_id);
    pb_data->mut_obj().set_bv_id(rel->bv_id);
    _dimension_ba_bv_relation_events.insert({rel->ba_id, pb_data});
  }
}

/**
 *  Process a dimension bv event
 *
 *  @param data  The event.
 */
void macro_cache::_process_dimension_bv_event(
    std::shared_ptr<io::data> const& data) {
  if (data->type() == bam::pb_dimension_bv_event::static_type()) {
    const auto& dbve =
        std::static_pointer_cast<bam::pb_dimension_bv_event>(data);
    _dimension_bv_events[dbve->obj().bv_id()] = dbve;
  } else if (data->type() == bam::dimension_bv_event::static_type()) {
    const auto& old_ev =
        std::static_pointer_cast<bam::dimension_bv_event>(data);
    auto ev = std::make_shared<bam::pb_dimension_bv_event>();
    ev->mut_obj().set_bv_id(old_ev->bv_id);
    ev->mut_obj().set_bv_name(old_ev->bv_name);
    ev->mut_obj().set_bv_description(old_ev->bv_description);
    _dimension_bv_events[ev->obj().bv_id()] = ev;
  }
}

/**
 *  Process a dimension truncate table signal
 *
 * @param data  The event.
 */
void macro_cache::_process_dimension_truncate_table_signal(
    std::shared_ptr<io::data> const& data) {
  auto const& trunc =
      std::static_pointer_cast<bam::dimension_truncate_table_signal>(data);
  SPDLOG_LOGGER_DEBUG(_cache->logger(),
                      "lua: processing dimension truncate table signal");

  if (trunc->update_started) {
    _dimension_ba_events.clear();
    _dimension_ba_bv_relation_events.clear();
    _dimension_bv_events.clear();
  }
}

/**
 *  Process a dimension truncate table signal
 *
 * @param data  The event.
 */
void macro_cache::_process_pb_dimension_truncate_table_signal(
    std::shared_ptr<io::data> const& data) {
  SPDLOG_LOGGER_DEBUG(_cache->logger(),
                      "lua: processing dimension truncate table signal");

  if (std::static_pointer_cast<bam::pb_dimension_truncate_table_signal>(data)
          ->obj()
          .update_started()) {
    _dimension_ba_events.clear();
    _dimension_ba_bv_relation_events.clear();
    _dimension_bv_events.clear();
  }
}

/**
 *  Process a custom variable event.
 *  The goal is to keep in cache only custom variables concerning severity on
 *  hosts and services.
 *
 *  @param data  The event.
 */
void macro_cache::_process_custom_variable(
    std::shared_ptr<io::data> const& data) {
  auto const& cv = std::static_pointer_cast<neb::custom_variable>(data);
  if (cv->name == "CRITICALITY_LEVEL") {
    SPDLOG_LOGGER_DEBUG(
        _cache->logger(),
        "lua: processing custom variable representing a criticality level for "
        "host_id {} and service_id {} and level {}",
        cv->host_id, cv->service_id, cv->value);
    int32_t value = std::atoi(cv->value.c_str());
    if (value)
      _custom_vars[{cv->host_id, cv->service_id}] = cv;
  }
}

/**
 *  Process a custom variable event.
 *  The goal is to keep in cache only custom variables concerning severity on
 *  hosts and services.
 *
 *  @param data  The event.
 */
void macro_cache::_process_pb_custom_variable(
    std::shared_ptr<io::data> const& data) {
  neb::pb_custom_variable::shared_ptr cv =
      std::static_pointer_cast<neb::pb_custom_variable>(data);
  if (cv->obj().name() == "CRITICALITY_LEVEL") {
    int32_t value;
    if (absl::SimpleAtoi(cv->obj().value(), &value)) {
      SPDLOG_LOGGER_DEBUG(_cache->logger(),
                          "lua: processing custom variable representing a "
                          "criticality level for "
                          "host_id {} and service_id {} and level {}",
                          cv->obj().host_id(), cv->obj().service_id(), value);
      if (value)
        _custom_vars[{cv->obj().host_id(), cv->obj().service_id()}] = cv;
    } else {
      SPDLOG_LOGGER_ERROR(_cache->logger(),
                          "lua: processing custom variable representing a "
                          "criticality level for "
                          "host_id {} and service_id {} incorrect value {}",
                          cv->obj().host_id(), cv->obj().service_id(),
                          cv->obj().value());
    }
  }
}

/**
 *  Save all data to disk.
 */
void macro_cache::_save_to_disk() {
  _cache->transaction();

  for (auto it(_instances.begin()), end(_instances.end()); it != end; ++it)
    _cache->add(it->second);

  for (auto it(_hosts.begin()), end(_hosts.end()); it != end; ++it)
    _cache->add(it->second);

  for (auto it(_host_groups.begin()), end(_host_groups.end()); it != end; ++it)
    _cache->add(it->second);

  for (auto it(_host_group_members.begin()), end(_host_group_members.end());
       it != end; ++it)
    _cache->add(it->second);

  for (auto it(_services.begin()), end(_services.end()); it != end; ++it)
    _cache->add(it->second);

  for (auto it(_service_groups.begin()), end(_service_groups.end()); it != end;
       ++it)
    _cache->add(it->second);

  for (auto it = _service_group_members.begin(),
            end = _service_group_members.end();
       it != end; ++it)
    _cache->add(it->second);

  for (auto it(_index_mappings.begin()), end(_index_mappings.end()); it != end;
       ++it)
    _cache->add(it->second);

  for (auto it = _metric_mappings.begin(), end = _metric_mappings.end();
       it != end; ++it)
    _cache->add(it->second);

  for (auto it(_dimension_ba_events.begin()), end(_dimension_ba_events.end());
       it != end; ++it)
    _cache->add(it->second);

  for (auto it(_dimension_ba_bv_relation_events.begin()),
       end(_dimension_ba_bv_relation_events.end());
       it != end; ++it)
    _cache->add(it->second);

  for (auto it(_dimension_bv_events.begin()), end(_dimension_bv_events.end());
       it != end; ++it)
    _cache->add(it->second);

  for (auto it = _custom_vars.begin(), end = _custom_vars.end(); it != end;
       ++it)
    _cache->add(it->second);
  _cache->commit();
}
