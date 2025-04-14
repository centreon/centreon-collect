/**
 * Copyright 2017-2022 Centreon
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

#include "com/centreon/broker/lua/macro_cache.hh"
#include <absl/strings/str_split.h>

#include "com/centreon/broker/neb/bbdo2_to_bbdo3.hh"

using namespace com::centreon::exceptions;
using namespace com::centreon::broker;
using namespace com::centreon::broker::lua;

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
      write(neb::bbdo2_to_bbdo3(d));
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
const std::shared_ptr<neb::pb_service>& macro_cache::get_service(
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
const std::shared_ptr<neb::pb_host>& macro_cache::get_host(
    uint64_t host_id) const {
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

  return found->second->obj().name();
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
std::string_view macro_cache::get_check_command(uint64_t host_id,
                                                uint64_t service_id) const {
  /* Case of services */
  std::string_view retval;
  if (service_id) {
    auto found = _services.find({host_id, service_id});
    if (found == _services.end())
      throw msg_fmt(
          "lua: could not find the check command of the service (host_id: {}, "
          "service_id: {})",
          host_id, service_id);
    neb::pb_service& s = static_cast<neb::pb_service&>(*found->second);
    retval = s.obj().check_command();
  }
  /* Case of hosts */
  else {
    auto found = _hosts.find(host_id);
    if (found == _hosts.end())
      throw msg_fmt(
          "lua: could not find the check command of the host (host_id: {})",
          host_id);
    neb::pb_host& s = static_cast<neb::pb_host&>(*found->second);
    retval = s.obj().check_command();
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
    return found->second->obj().notes_url();
  } else {
    auto found = _hosts.find(host_id);

    if (found == _hosts.end())
      throw msg_fmt("lua: could not find information on host {}", host_id);

    return found->second->obj().notes_url();
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
    return found->second->obj().action_url();
  } else {
    auto found = _hosts.find(host_id);

    if (found == _hosts.end())
      throw msg_fmt("lua: could not find information on host {}", host_id);

    return found->second->obj().action_url();
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
    return found->second->obj().notes();
  } else {
    auto found = _hosts.find(host_id);

    if (found == _hosts.end())
      throw msg_fmt("lua: could not find information on host {}", host_id);
    return found->second->obj().notes();
  }
}

/**
 *  Get a map of the host groups members index by host_id and host_group.
 *
 *  @return             A std::map
 */
const absl::btree_map<std::pair<uint64_t, uint64_t>, std::shared_ptr<io::data>>&
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
const std::string& macro_cache::get_host_group_name(uint64_t id) const {
  const auto found = _host_groups.find(id);

  if (found == _host_groups.end()) {
    _cache->logger()->error("lua: could not find information on host group {}",
                            id);
    throw msg_fmt("lua: could not find information on host group {}", id);
  }
  return found->second.first->obj().name();
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
  return found->second->obj().description();
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
                std::shared_ptr<io::data>> const&
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

  if (found == _service_groups.end()) {
    _cache->logger()->error(
        "lua: could not find information on service group {}", id);
    throw msg_fmt("lua: could not find information on service group {}", id);
  }
  return found->second.first->obj().name();
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
  return std::static_pointer_cast<neb::pb_instance>(found->second)
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
void macro_cache::write(const std::shared_ptr<io::data>& d) {
  if (!d)
    return;

  std::shared_ptr<io::data> data = neb::bbdo2_to_bbdo3(d);

  switch (data->type()) {
    case neb::pb_instance::static_type():
      _process_pb_instance(data);
      break;
    case neb::pb_host::static_type():
      _process_pb_host(data);
      break;
    case neb::pb_host_status::static_type():
      _process_pb_host_status(data);
      break;
    case neb::pb_adaptive_host::static_type():
      _process_pb_adaptive_host(data);
      break;
    case neb::pb_adaptive_host_status::static_type():
      _process_pb_adaptive_host_status(data);
      break;
    case neb::pb_host_group::static_type():
      _process_pb_host_group(data);
      break;
    case neb::pb_host_group_member::static_type():
      _process_pb_host_group_member(data);
      break;
    case neb::pb_service::static_type():
      _process_pb_service(data);
      break;
    case neb::pb_service_status::static_type():
      _process_pb_service_status(data);
      break;
    case neb::pb_adaptive_service_status::static_type():
      _process_pb_adaptive_service_status(data);
      break;
    case neb::pb_adaptive_service::static_type():
      _process_pb_adaptive_service(data);
      break;
    case neb::pb_service_group::static_type():
      _process_pb_service_group(data);
      break;
    case neb::pb_service_group_member::static_type():
      _process_pb_service_group_member(data);
      break;
    case neb::pb_custom_variable::static_type():
      _process_pb_custom_variable(data);
      break;
    case storage::pb_index_mapping::static_type():
      _process_index_mapping(data);
      break;
    case storage::pb_metric_mapping::static_type():
      _process_metric_mapping(data);
      break;
    case bam::pb_dimension_ba_event::static_type():
      _process_dimension_ba_event(data);
      break;
    case bam::pb_dimension_ba_bv_relation_event::static_type():
      _process_dimension_ba_bv_relation_event(data);
      break;
    case bam::pb_dimension_bv_event::static_type():
      _process_dimension_bv_event(data);
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
void macro_cache::_process_pb_instance(std::shared_ptr<io::data> const& data) {
  auto const& in = std::static_pointer_cast<neb::pb_instance>(data);
  _instances[in->obj().instance_id()] = in;
}

/**
 *  Process a pb host event.
 *
 *  @param h  The event.
 */
void macro_cache::_process_pb_host(std::shared_ptr<io::data> const& data) {
  const auto& h = std::static_pointer_cast<neb::pb_host>(data);
  SPDLOG_LOGGER_DEBUG(_cache->logger(),
                      "lua: processing host '{}' of id {} enabled {}",
                      h->obj().name(), h->obj().host_id(), h->obj().enabled());
  if (h->obj().enabled())
    _hosts[h->obj().host_id()] = h;
  else
    _hosts.erase(h->obj().host_id());
}

void macro_cache::_process_pb_host_status(
    const std::shared_ptr<io::data>& data) {
  const auto& s = std::static_pointer_cast<neb::pb_host_status>(data);
  const auto& obj = s->obj();

  SPDLOG_LOGGER_DEBUG(_cache->logger(), "lua: processing host status ({})",
                      obj.host_id());

  auto it = _hosts.find(obj.host_id());
  if (it == _hosts.end()) {
    _cache->logger()->warn(
        "lua: Attempt to update host ({}) in lua cache, but it does not "
        "exist. Maybe Engine should be restarted to update the cache.",
        obj.host_id());
    return;
  }

  auto& hst = std::static_pointer_cast<neb::pb_host>(it->second)->mut_obj();
  hst.set_checked(obj.checked());
  hst.set_check_type(static_cast<Host_CheckType>(obj.check_type()));
  hst.set_state(static_cast<Host_State>(obj.state()));
  hst.set_state_type(static_cast<Host_StateType>(obj.state_type()));
  hst.set_last_state_change(obj.last_state_change());
  hst.set_last_hard_state(static_cast<Host_State>(obj.last_hard_state()));
  hst.set_last_hard_state_change(obj.last_hard_state_change());
  hst.set_last_time_up(obj.last_time_up());
  hst.set_last_time_down(obj.last_time_down());
  hst.set_last_time_unreachable(obj.last_time_unreachable());
  hst.set_output(obj.output());
  hst.set_perfdata(obj.perfdata());
  hst.set_flapping(obj.flapping());
  hst.set_percent_state_change(obj.percent_state_change());
  hst.set_latency(obj.latency());
  hst.set_execution_time(obj.execution_time());
  hst.set_last_check(obj.last_check());
  hst.set_next_check(obj.next_check());
  hst.set_should_be_scheduled(obj.should_be_scheduled());
  hst.set_check_attempt(obj.check_attempt());
  hst.set_notification_number(obj.notification_number());
  hst.set_no_more_notifications(obj.no_more_notifications());
  hst.set_last_notification(obj.last_notification());
  hst.set_next_host_notification(obj.next_host_notification());
  hst.set_acknowledgement_type(obj.acknowledgement_type());
  hst.set_scheduled_downtime_depth(obj.scheduled_downtime_depth());
}

/**
 * @brief Process a pb adaptive host event.
 *
 * @param data An AdaptiveHostStatus event.
 */
void macro_cache::_process_pb_adaptive_host_status(
    const std::shared_ptr<io::data>& data) {
  const auto& s = std::static_pointer_cast<neb::pb_adaptive_host_status>(data);
  const auto& obj = s->obj();

  SPDLOG_LOGGER_DEBUG(_cache->logger(),
                      "lua: processing adaptive host status ({})",
                      obj.host_id());

  auto it = _hosts.find(obj.host_id());
  if (it == _hosts.end()) {
    _cache->logger()->warn(
        "lua: Attempt to update host ({}) in lua cache, but it does not "
        "exist. Maybe Engine should be restarted to update the cache.",
        obj.host_id());
    return;
  }

  auto& hst = std::static_pointer_cast<neb::pb_host>(it->second)->mut_obj();
  if (obj.has_scheduled_downtime_depth())
    hst.set_scheduled_downtime_depth(obj.scheduled_downtime_depth());
  if (obj.has_acknowledgement_type())
    hst.set_acknowledgement_type(obj.acknowledgement_type());
  if (obj.has_notification_number())
    hst.set_notification_number(obj.notification_number());
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
    auto& h = it->second->mut_obj();
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
void macro_cache::_process_pb_host_group(
    const std::shared_ptr<io::data>& data) {
  auto pb_hg = std::static_pointer_cast<neb::pb_host_group>(data);
  const HostGroup& hg = pb_hg->obj();
  SPDLOG_LOGGER_DEBUG(_cache->logger(),
                      "lua: processing pb host group '{}' of id {}, enabled {}",
                      hg.name(), hg.hostgroup_id(), hg.enabled());
  if (hg.enabled()) {
    auto found = _host_groups.find(hg.hostgroup_id());
    if (found != _host_groups.end()) {
      found->second.second.insert(hg.poller_id());
      HostGroup& current_hg =
          std::static_pointer_cast<neb::pb_host_group>(found->second.first)
              ->mut_obj();
      current_hg.set_name(hg.name());
    } else {
      absl::flat_hash_set<uint32_t> pollers{hg.poller_id()};
      _host_groups[hg.hostgroup_id()] =
          std::make_pair(std::move(pb_hg), pollers);
    }
  } else {
    /* We check that no more pollers need this host group. So if the set is
     * empty, we can also remove the host group. */
    auto found = _host_groups.find(hg.hostgroup_id());
    if (found != _host_groups.end()) {
      auto f = found->second.second.find(hg.poller_id());
      if (f != found->second.second.end()) {
        found->second.second.erase(f);
        if (found->second.second.empty()) {
          _host_groups.erase(found);
        }
      }
    }
  }
}

/**
 *  Process a host group member event.
 *
 *  @param data  The event.
 */
void macro_cache::_process_pb_host_group_member(
    std::shared_ptr<io::data> const& data) {
  const HostGroupMember& hgm =
      std::static_pointer_cast<neb::pb_host_group_member>(data)->obj();
  SPDLOG_LOGGER_DEBUG(
      _cache->logger(),
      "lua: processing pb host group member (group_name: '{}', group_id: {}, "
      "host_id: {}, enabled: {})",
      hgm.name(), hgm.hostgroup_id(), hgm.host_id(), hgm.enabled());
  if (hgm.enabled())
    _host_group_members[{hgm.host_id(), hgm.hostgroup_id()}] = data;
  else
    _host_group_members.erase({hgm.host_id(), hgm.hostgroup_id()});
}

/**
 *  Process a pb service event.
 *
 *  @param s  The event.
 */
void macro_cache::_process_pb_service(std::shared_ptr<io::data> const& data) {
  auto const& s = std::static_pointer_cast<neb::pb_service>(data);
  SPDLOG_LOGGER_DEBUG(
      _cache->logger(),
      "lua: processing service ({}, {}) (description:{}) enabled {}",
      s->obj().host_id(), s->obj().service_id(), s->obj().description(),
      s->obj().enabled());
  if (s->obj().enabled())
    _services[{s->obj().host_id(), s->obj().service_id()}] = s;
  else
    _services.erase({s->obj().host_id(), s->obj().service_id()});
}

/**
 * @brief Process a pb adaptive service event.
 *
 * @param data An AdaptiveServiceStatus event.
 */
void macro_cache::_process_pb_adaptive_service_status(
    const std::shared_ptr<io::data>& data) {
  const auto& s =
      std::static_pointer_cast<neb::pb_adaptive_service_status>(data);
  const auto& obj = s->obj();

  SPDLOG_LOGGER_DEBUG(_cache->logger(),
                      "lua: processing adaptive service status ({}, {})",
                      obj.host_id(), obj.service_id());

  auto it = _services.find({obj.host_id(), obj.service_id()});
  if (it == _services.end()) {
    _cache->logger()->warn(
        "lua: Attempt to update service ({}, {}) in lua cache, but it does not "
        "exist. Maybe Engine should be restarted to update the cache.",
        obj.host_id(), obj.service_id());
    return;
  }

  auto& svc = std::static_pointer_cast<neb::pb_service>(it->second)->mut_obj();
  if (obj.has_acknowledgement_type())
    svc.set_acknowledgement_type(obj.acknowledgement_type());
  if (obj.has_scheduled_downtime_depth())
    svc.set_scheduled_downtime_depth(obj.scheduled_downtime_depth());
  if (obj.has_notification_number())
    svc.set_notification_number(obj.notification_number());
}

void macro_cache::_process_pb_service_status(
    const std::shared_ptr<io::data>& data) {
  const auto& s = std::static_pointer_cast<neb::pb_service_status>(data);
  const auto& obj = s->obj();

  SPDLOG_LOGGER_DEBUG(_cache->logger(),
                      "lua: processing service status ({}, {})", obj.host_id(),
                      obj.service_id());

  auto it = _services.find({obj.host_id(), obj.service_id()});
  if (it == _services.end()) {
    _cache->logger()->warn(
        "lua: Attempt to update service ({}, {}) in lua cache, but it does not "
        "exist. Maybe Engine should be restarted to update the cache.",
        obj.host_id(), obj.service_id());
    return;
  }

  auto& svc = it->second->mut_obj();
  svc.set_checked(obj.checked());
  svc.set_check_type(static_cast<Service_CheckType>(obj.check_type()));
  svc.set_state(static_cast<Service_State>(obj.state()));
  svc.set_state_type(static_cast<Service_StateType>(obj.state_type()));
  svc.set_last_state_change(obj.last_state_change());
  svc.set_last_hard_state(static_cast<Service_State>(obj.last_hard_state()));
  svc.set_last_hard_state_change(obj.last_hard_state_change());
  svc.set_last_time_ok(obj.last_time_ok());
  svc.set_last_time_warning(obj.last_time_warning());
  svc.set_last_time_critical(obj.last_time_critical());
  svc.set_last_time_unknown(obj.last_time_unknown());
  svc.set_output(obj.output());
  svc.set_perfdata(obj.perfdata());
  svc.set_flapping(obj.flapping());
  svc.set_percent_state_change(obj.percent_state_change());
  svc.set_latency(obj.latency());
  svc.set_execution_time(obj.execution_time());
  svc.set_last_check(obj.last_check());
  svc.set_next_check(obj.next_check());
  svc.set_should_be_scheduled(obj.should_be_scheduled());
  svc.set_check_attempt(obj.check_attempt());
  svc.set_notification_number(obj.notification_number());
  svc.set_no_more_notifications(obj.no_more_notifications());
  svc.set_last_notification(obj.last_notification());
  svc.set_next_notification(obj.next_notification());
  svc.set_acknowledgement_type(obj.acknowledgement_type());
  svc.set_scheduled_downtime_depth(obj.scheduled_downtime_depth());
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
    auto& s = it->second->mut_obj();
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
void macro_cache::_process_pb_service_group(
    const std::shared_ptr<io::data>& data) {
  auto pb_sg = std::static_pointer_cast<neb::pb_service_group>(data);
  const ServiceGroup& sg = pb_sg->obj();
  SPDLOG_LOGGER_DEBUG(_cache->logger(),
                      "lua: processing pb service group '{}' of id {}",
                      sg.name(), sg.servicegroup_id());
  if (sg.enabled()) {
    auto found = _service_groups.find(sg.servicegroup_id());
    if (found != _service_groups.end()) {
      found->second.second.insert(sg.poller_id());
      ServiceGroup& current_sg = found->second.first->mut_obj();
      current_sg.set_name(sg.name());
    } else {
      /* Here, we add the servicegroup and the first poller that needs it */
      absl::flat_hash_set<uint32_t> pollers{sg.poller_id()};
      _service_groups[sg.servicegroup_id()] =
          std::make_pair(std::move(pb_sg), pollers);
    }
  } else {
    /* We check that no more pollers need this service group. So if the set is
     * empty, we can also remove the service group. */
    auto found = _service_groups.find(sg.servicegroup_id());
    if (found != _service_groups.end()) {
      auto f = found->second.second.find(sg.poller_id());
      if (f != found->second.second.end()) {
        found->second.second.erase(f);
        if (found->second.second.empty()) {
          _service_groups.erase(found);
        }
      }
    }
  }
}

/**
 *  Process a service group member event.
 *
 *  @param data  The event.
 */
void macro_cache::_process_pb_service_group_member(
    std::shared_ptr<io::data> const& data) {
  const ServiceGroupMember& sgm =
      std::static_pointer_cast<neb::pb_service_group_member>(data)->obj();
  SPDLOG_LOGGER_DEBUG(_cache->logger(),
                      "lua: processing pb service group member (group_name: "
                      "{}, group_id: {}, "
                      "host_id: {}, service_id: {} enabled: {}",
                      sgm.name(), sgm.servicegroup_id(), sgm.host_id(),
                      sgm.service_id(), sgm.enabled());
  if (sgm.enabled())
    _service_group_members[std::make_tuple(sgm.host_id(), sgm.service_id(),
                                           sgm.servicegroup_id())] = data;
  else
    _service_group_members.erase(std::make_tuple(
        sgm.host_id(), sgm.service_id(), sgm.servicegroup_id()));
}

/**
 *  Process an index mapping event.
 *
 *  @param im  The event.
 */
void macro_cache::_process_index_mapping(
    std::shared_ptr<io::data> const& data) {
  auto im = std::static_pointer_cast<storage::pb_index_mapping>(data);
  _index_mappings[im->obj().index_id()] = im;
}

/**
 *  Process a metric mapping event.
 *
 *  @param data  The event.
 */
void macro_cache::_process_metric_mapping(
    std::shared_ptr<io::data> const& data) {
  const auto& mm = std::static_pointer_cast<storage::pb_metric_mapping>(data);
  _metric_mappings[mm->obj().metric_id()] = mm;
}

/**
 *  Process a dimension ba event
 *
 *  @param data  The event.
 */
void macro_cache::_process_dimension_ba_event(
    std::shared_ptr<io::data> const& data) {
  auto const& dbae = std::static_pointer_cast<bam::pb_dimension_ba_event>(data);
  SPDLOG_LOGGER_DEBUG(_cache->logger(),
                      "lua: pb processing dimension ba event of id {}",
                      dbae->obj().ba_id());
  _dimension_ba_events[dbae->obj().ba_id()] = dbae;
}

/**
 *  Process a dimension ba bv relation event
 *
 *  @param data  The event.
 */
void macro_cache::_process_dimension_ba_bv_relation_event(
    std::shared_ptr<io::data> const& data) {
  const auto& pb_data =
      std::static_pointer_cast<bam::pb_dimension_ba_bv_relation_event>(data);
  const DimensionBaBvRelationEvent& rel = pb_data->obj();
  SPDLOG_LOGGER_DEBUG(_cache->logger(),
                      "lua: processing pb dimension ba bv relation event "
                      "(ba_id: {}, bv_id: {})",
                      rel.ba_id(), rel.bv_id());
  _dimension_ba_bv_relation_events.insert({rel.ba_id(), pb_data});
}

/**
 *  Process a dimension bv event
 *
 *  @param data  The event.
 */
void macro_cache::_process_dimension_bv_event(
    std::shared_ptr<io::data> const& data) {
  const auto& dbve = std::static_pointer_cast<bam::pb_dimension_bv_event>(data);
  _dimension_bv_events[dbve->obj().bv_id()] = dbve;
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

  for (auto it = _host_groups.begin(), end = _host_groups.end(); it != end;
       ++it) {
    for (auto poller_id : it->second.second) {
      it->second.first->mut_obj().set_poller_id(poller_id);
      _cache->add(it->second.first);
    }
  }

  for (auto it(_host_group_members.begin()), end(_host_group_members.end());
       it != end; ++it)
    _cache->add(it->second);

  for (auto it(_services.begin()), end(_services.end()); it != end; ++it)
    _cache->add(it->second);

  for (auto it = _service_groups.begin(), end = _service_groups.end();
       it != end; ++it) {
    for (auto poller_id : it->second.second) {
      it->second.first->mut_obj().set_poller_id(poller_id);
      _cache->add(it->second.first);
    }
  }

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
