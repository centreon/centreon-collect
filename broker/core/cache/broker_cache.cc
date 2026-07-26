/**
 * Copyright 2025-2026 Centreon
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
#include <boost/preprocessor/seq/for_each.hpp>

#include <array>
#include <memory>
#include "absl/synchronization/mutex.h"
#include "bbdo/bam/dimension_ba_bv_relation_event.hh"
#include "bbdo/storage/index_mapping.hh"

#include "broker/core/config/applier/state.hh"
#include "com/centreon/broker/multiplexing/publisher.hh"
#include "com/centreon/broker/neb/bbdo2_to_bbdo3.hh"
#include "common/downtimes/downtime_manager.hh"
#include "common/engine_conf/hostdependency_helper.hh"
#include "common/engine_conf/servicedependency_helper.hh"
#include "common/timeperiods/timezone.hh"

namespace com::centreon::broker::cache {

/**
 * @brief Constructor
 *
 * @param logger Logger instance
 */
broker_cache::broker_cache(std::shared_ptr<spdlog::logger> logger)
    : _logger{std::move(logger)} {  // logger: log_v2::CACHE
  const auto& cache_dir = config::applier::state::instance().cache_dir();
  if (cache_dir.empty()) {
    _cache_file = std::filesystem::temp_directory_path() /
                  fmt::format("broker_cache_{}.prot", ::getpid());
    SPDLOG_LOGGER_DEBUG(_logger,
                        "broker_cache: cache directory not set, using '{}'",
                        _cache_file.string());
  } else {
    _cache_file = std::filesystem::path{cache_dir + ".cache"};
  }

  /* In legacy mode the whole cache is reloaded; in centralized mode only the
   * persisted active downtimes are read back (the rest of the cache is rebuilt
   * from the Engine configuration). _load_cache() handles both: it applies the
   * heavy sections only in legacy mode but always reads active_downtimes. */
  _load_cache();
}

/**
 * @brief Destructor
 */
broker_cache::~broker_cache() noexcept {
  /* Save the cache. In legacy mode the whole cache is persisted; in centralized
   * mode _save_cache() persists only the active downtimes (the rest is rebuilt
   * from the Engine configuration on restart). */
  _save_cache();
}

/**
 * @brief Swap the hostgroup stored at @a found for @a replacement.
 *
 * Cached hostgroups must never be modified in place: the getters hand out the
 * stored `shared_ptr` and readers keep using it after having released the
 * mutex, so mutating the pointee would be a data race. Callers therefore clone
 * the group, apply their changes to the clone, and hand it over here.
 *
 * Two containers alias the same `pb_host_group`: `_hostgroups` owns it and
 * `_host_hostgroups` references it for each member host. Both are rebound here,
 * otherwise readers of the relations would keep seeing the previous version of
 * the group.
 *
 * `modify()` recomputes the keys of every index, so a renaming is correctly
 * reflected in the `by_name` index. Should the new name collide with another
 * group, the rollback restores the previous pointer and the element stays in
 * the container.
 *
 * @param found       Iterator, in the `by_id` index, of the group to replace.
 * @param replacement The new version of the group. Its ID must be unchanged.
 *
 * @return True on success, false if the replacement violates a uniqueness
 *         constraint, in which case the cache is left untouched.
 */
bool broker_cache::_replace_hostgroup(
    HostgroupContainer::index<by_id>::type::iterator found,
    const std::shared_ptr<neb::pb_host_group>& replacement) {
  auto previous = found->first;
  if (!_hostgroups.get<by_id>().modify(
          found, [&replacement](auto& p) { p.first = replacement; },
          [&previous](auto& p) { p.first = previous; }))
    return false;

  /* The hostgroup ID is unchanged, so no key of _host_hostgroups is impacted
   * and these modify() calls cannot fail. */
  auto& by_hg = _host_hostgroups.get<by_hostgroup>();
  auto [lower, upper] = by_hg.equal_range(replacement->obj().hostgroup_id());
  for (auto it = lower; it != upper; ++it)
    by_hg.modify(it, [&replacement](indexed_host_hostgroup& hh) {
      hh.hostgroup = replacement;
    });
  return true;
}

/**
 * @brief Swap the servicegroup stored at @a found for @a replacement.
 *
 * Servicegroup counterpart of _replace_hostgroup(); see its documentation for
 * the rationale. The aliasing container is `_service_servicegroups` here.
 *
 * @param found       Iterator, in the `by_id` index, of the group to replace.
 * @param replacement The new version of the group. Its ID must be unchanged.
 *
 * @return True on success, false if the replacement violates a uniqueness
 *         constraint, in which case the cache is left untouched.
 */
bool broker_cache::_replace_servicegroup(
    ServicegroupContainer::index<by_id>::type::iterator found,
    const std::shared_ptr<neb::pb_service_group>& replacement) {
  auto previous = found->first;
  if (!_servicegroups.get<by_id>().modify(
          found, [&replacement](auto& p) { p.first = replacement; },
          [&previous](auto& p) { p.first = previous; }))
    return false;

  /* The servicegroup ID is unchanged, so no key of _service_servicegroups is
   * impacted and these modify() calls cannot fail. */
  auto& by_sg = _service_servicegroups.get<by_servicegroup>();
  auto [lower, upper] = by_sg.equal_range(replacement->obj().servicegroup_id());
  for (auto it = lower; it != upper; ++it)
    by_sg.modify(it, [&replacement](indexed_service_servicegroup& ssg) {
      ssg.servicegroup = replacement;
    });
  return true;
}

/**
 * @brief Merge a configuration state into the cache. Used when a poller
 * established a connection to Broker (i.e. when a `pb_engine_state` event is
 * received).
 *
 * In centralized configuration mode the protobuf `State` message carries a
 * top-level `poller_id` field, but the individual `Hostgroup` and
 * `Servicegroup` objects embedded inside `state` do NOT have their own
 * `poller_id` set (Engine has no concept of per-object poller ownership).
 * `state.poller_id()` is used as fallback wherever `obj.poller_id() == 0`.
 *
 * Similarly, individual `Host` objects do not carry their own `poller_id`;
 * `state.poller_id()` is forwarded to `_fill_host()` as the `poller_id_hint`.
 *
 * @param state Configuration state to merge
 */
void broker_cache::merge(
    const com::centreon::engine::configuration::State& state) {
  absl::WriterMutexLock lck{&_mutex};

  /* Work on instances */
  if (section_enabled(CACHE_INSTANCES))
    _instances.insert_or_assign(
        state.poller_id(),
        instance_info{state.poller_name(), state.enable_notifications(),
                      state.send_recovery_notifications_anyway(),
                      state.interval_length() > 0
                          ? std::chrono::seconds(state.interval_length())
                          : instance_info::default_interval_length,
                      state.soft_state_dependencies()});

  /* Work on timeperiods */
  if (section_enabled(CACHE_NOTIFICATIONS) && !state.timeperiods().empty()) {
    for (const engine::configuration::Timeperiod& tp : state.timeperiods()) {
      _timeperiods.insert_or_assign(
          tp.timeperiod_name(),
          std::make_shared<common::timeperiods::timeperiod>(tp, _logger));
      _timeperiod_pollers[tp.timeperiod_name()].insert(state.poller_id());
    }
    _resolve_timeperiods();
  }

  /* Work on notification contacts and contactgroups. Reference-counted per
   * poller like the timeperiods above. A full state rebuilds this poller's
   * contribution: drop it first, then re-insert, so contacts/contactgroups
   * removed from the poller's configuration do not linger. Names are stored
   * as-is; contactgroups are expanded to contacts lazily at notification time
   * (see contactgroup_members()), so no resolution is done here. */
  if (section_enabled(CACHE_NOTIFICATIONS)) {
    _remove_poller_from_contacts(state.poller_id());
    for (const engine::configuration::Contact& c : state.contacts())
      _insert_contact(c, state.poller_id());
    for (const engine::configuration::Contactgroup& cg : state.contactgroups())
      _insert_contactgroup(cg, state.poller_id());
  }

  /* Work on severities */
  if (section_enabled(CACHE_SEVERITIES)) {
    for (const engine::configuration::Severity& sev : state.severities()) {
      auto key = std::make_pair(sev.key().id(), sev.key().type());
      uint64_t poller_id =
          sev.poller_id() != 0 ? sev.poller_id() : state.poller_id();
      auto it = _severities.find(key);
      if (it != _severities.end()) {
        it->second.first.level = sev.level();  // preserve existing db_id
        it->second.second.insert(poller_id);
      } else {
        _severities.insert(
            {key,
             {{sev.level(), 0}, absl::flat_hash_set<uint64_t>{poller_id}}});
      }
    }
  }

  /* Work on hosts */
  if (section_enabled(CACHE_HOSTS)) {
    auto& index = _hosts.get<by_id>();
    for (const engine::configuration::Host& host : state.hosts()) {
      auto h = std::make_shared<neb::pb_host>();
      _fill_host(&h->mut_obj(), host, state.poller_id());
      auto [it, inserted] = index.insert(h);
      if (!inserted)
        index.replace(it, h);
    }
  }

  /* Work on hostgroups */
  if (section_enabled(CACHE_GROUPS)) {
    auto& hg_index = _hostgroups.get<by_id>();
    auto fill_hostgroup = [](HostGroup& obj,
                             const engine::configuration::Hostgroup& hg) {
      obj.set_hostgroup_id(hg.hostgroup_id());
      obj.set_name(hg.hostgroup_name());
      obj.set_enabled(true);
      /* Hostgroups are linked to several pollers, so we set poller_id to 0.
       */
      obj.set_poller_id(0);
      obj.set_alias(hg.alias());
    };

    for (const auto& hg : state.hostgroups()) {
      const uint64_t hg_poller_id =
          hg.poller_id() != 0 ? hg.poller_id() : state.poller_id();
      auto found = hg_index.find(hg.hostgroup_id());
      bool inserted = false;
      if (found == hg_index.end()) {
        auto hostgroup = std::make_shared<neb::pb_host_group>();
        fill_hostgroup(hostgroup->mut_obj(), hg);
        std::tie(found, inserted) = hg_index.emplace(
            hostgroup, absl::flat_hash_set<uint64_t>{hg_poller_id});
      } else {
        /* The stored group is shared with readers that no longer hold the
         * mutex: clone it, change the clone, then swap. Pollers largely declare
         * the same groups, so the clone is skipped when the cached group
         * already carries the expected values. */
        auto current = found->first;
        if (current->obj().name() != hg.hostgroup_name() ||
            current->obj().alias() != hg.alias() || !current->obj().enabled()) {
          auto hostgroup = std::make_shared<neb::pb_host_group>(*current);
          auto& obj = hostgroup->mut_obj();
          obj.set_name(hg.hostgroup_name());
          obj.set_enabled(true);
          obj.set_alias(hg.alias());
          if (!_replace_hostgroup(found, hostgroup))
            SPDLOG_LOGGER_ERROR(
                _logger,
                "Failed to rename host group {} to '{}': another host group "
                "already uses this name",
                hg.hostgroup_id(), hg.hostgroup_name());
        }
        /* The poller set belongs to the container node and never escapes, so
         * the mutex alone protects it. */
        const_cast<absl::flat_hash_set<uint64_t>&>(found->second)
            .insert(hg_poller_id);
      }
      for (const auto& member : hg.members().data()) {
        auto& index = _hosts.get<by_name>();
        auto host_it = index.find(member);
        if (host_it == index.end())
          continue;

        uint64_t host_id = (*host_it)->obj().host_id();
        _host_hostgroups.insert({host_id, found->first});
      }
    }
  }

  /* Work on services */
  if (section_enabled(CACHE_SERVICES)) {
    auto& index_svc = _services.get<by_id>();
    for (const engine::configuration::Service& svc : state.services()) {
      auto s = std::make_shared<neb::pb_service>();
      _fill_service(&s->mut_obj(), svc);
      auto [it, inserted] = index_svc.insert(s);
      if (!inserted)
        index_svc.replace(it, s);
    }

    /* Work on anomaly detections */
    _anomaly_detection_index.clear();
    for (const engine::configuration::Anomalydetection& ad :
         state.anomalydetections()) {
      auto s = std::make_shared<neb::pb_service>();
      _fill_anomaly_detection(&s->mut_obj(), ad);
      auto [it, inserted] = index_svc.insert(s);
      if (!inserted)
        index_svc.replace(it, s);
      _anomaly_detection_index[std::make_pair(ad.host_id(),
                                              ad.dependent_service_id())]
          .insert(ad.service_id());
    }
  }

  /* Work on servicegroups */
  if (section_enabled(CACHE_GROUPS)) {
    auto& sg_index = _servicegroups.get<by_id>();
    auto fill_servicegroup = [](ServiceGroup& obj,
                                const engine::configuration::Servicegroup& sg) {
      obj.set_servicegroup_id(sg.servicegroup_id());
      obj.set_name(sg.servicegroup_name());
      obj.set_enabled(true);
      obj.set_poller_id(sg.poller_id());
      obj.set_alias(sg.alias());
    };

    for (const auto& sg : state.servicegroups()) {
      const uint64_t sg_poller_id =
          sg.poller_id() != 0 ? sg.poller_id() : state.poller_id();
      auto found = sg_index.find(sg.servicegroup_id());
      bool inserted = false;
      if (found == sg_index.end()) {
        auto servicegroup = std::make_shared<neb::pb_service_group>();
        fill_servicegroup(servicegroup->mut_obj(), sg);
        std::tie(found, inserted) = sg_index.emplace(
            servicegroup, absl::flat_hash_set<uint64_t>{sg_poller_id});
      } else {
        /* The stored group is shared with readers that no longer hold the
         * mutex: clone it, change the clone, then swap. Pollers largely declare
         * the same groups, so the clone is skipped when the cached group
         * already carries the expected values. */
        auto current = found->first;
        if (current->obj().name() != sg.servicegroup_name() ||
            current->obj().alias() != sg.alias() || !current->obj().enabled()) {
          auto servicegroup = std::make_shared<neb::pb_service_group>(*current);
          auto& obj = servicegroup->mut_obj();
          obj.set_name(sg.servicegroup_name());
          obj.set_enabled(true);
          obj.set_alias(sg.alias());
          if (!_replace_servicegroup(found, servicegroup))
            SPDLOG_LOGGER_ERROR(
                _logger,
                "Failed to rename service group {} to '{}': another service "
                "group already uses this name",
                sg.servicegroup_id(), sg.servicegroup_name());
        }
        /* The poller set belongs to the container node and never escapes, so
         * the mutex alone protects it. */
        const_cast<absl::flat_hash_set<uint64_t>&>(found->second)
            .insert(sg_poller_id);
      }
      for (const auto& member : sg.members().data()) {
        auto& index = _services.get<by_name>();
        auto service_it =
            index.find(std::make_pair(member.first(), member.second()));
        if (service_it == index.end())
          continue;

        uint64_t host_id = (*service_it)->obj().host_id();
        uint64_t service_id = (*service_it)->obj().service_id();
        _service_servicegroups.insert({host_id, service_id, found->first});
      }
    }
  }

  /* Work on tags */
  if (section_enabled(CACHE_TAGS)) {
    for (const engine::configuration::Tag& tag : state.tags()) {
      auto pb = std::make_shared<neb::pb_tag>();
      auto& obj = pb->mut_obj();
      obj.set_id(tag.key().id());
      obj.set_type(static_cast<TagType>(tag.key().type()));
      obj.set_name(tag.tag_name());
      uint64_t tag_pid =
          tag.poller_id() != 0 ? tag.poller_id() : state.poller_id();
      obj.set_poller_id(tag_pid);
      obj.set_action(Tag_Action_ADD);
      auto key = std::make_pair(obj.id(), obj.type());
      auto [it, inserted] = _tags.emplace(
          key, std::make_pair(pb, absl::flat_hash_set<uint64_t>{tag_pid}));
      if (!inserted) {
        it->second.first = pb;
        it->second.second.insert(tag_pid);
      }
    }
  }

  /* Work on notification dependencies. Resolving the referenced hosts/services
   * to ids relies on the cache being populated above, so this runs last. A full
   * state means this poller's set is rebuilt in bloc: purge then re-insert.
   * CACHE_NOTIFICATIONS implies CACHE_HOSTS/CACHE_SERVICES, so the hosts and
   * services needed for the name->id resolution are guaranteed to be cached. */
  if (section_enabled(CACHE_NOTIFICATIONS)) {
    _host_notif_deps.get<by_instance>().erase(state.poller_id());
    for (const engine::configuration::Hostdependency& dep :
         state.hostdependencies())
      _insert_host_notif_dep(dep, state.poller_id());
    _service_notif_deps.get<by_instance>().erase(state.poller_id());
    for (const engine::configuration::Servicedependency& dep :
         state.servicedependencies())
      _insert_service_notif_dep(dep, state.poller_id());
  }
}

/**
 * @brief Resolve a notification host dependency to ids and store it.
 *
 * Only notification dependencies are kept. The dependency is post-expand, so it
 * references exactly one dependent host and one master host by name; both are
 * resolved to their ids through the existing by_name index. A name that cannot
 * be resolved (non-nominal, the expand/resolve step validated them intra-poller
 * beforehand) is logged and skipped.
 *
 * @param dep A post-expand Hostdependency.
 * @param poller_id The poller owning this dependency.
 */
void broker_cache::_insert_host_notif_dep(
    const engine::configuration::Hostdependency& dep,
    uint64_t poller_id) {
  if (dep.dependency_type() != engine::configuration::notification_dependency)
    return;

  const auto& by_name_idx = _hosts.get<by_name>();
  auto dependent = by_name_idx.find(dep.dependent_hosts().data(0));
  auto master = by_name_idx.find(dep.hosts().data(0));
  if (dependent == by_name_idx.end() || master == by_name_idx.end()) {
    _logger->warn(
        "broker_cache: cannot store notification host dependency of '{}' on "
        "'{}' for poller {}: host not found in cache",
        dep.dependent_hosts().data(0), dep.hosts().data(0), poller_id);
    return;
  }
  _host_notif_deps.insert(host_notif_dep{
      (*dependent)->obj().host_id(), (*master)->obj().host_id(), poller_id,
      dep.dependency_period(), dep.inherits_parent(),
      dep.notification_failure_options(), hostdependency_key(dep)});
}

/**
 * @brief Resolve a notification service dependency to ids and store it.
 *
 * Same contract as _insert_host_notif_dep, but the dependent and master are
 * services identified by their {host_name, service_description} pair, resolved
 * to {host_id, service_id} through the services by_name index.
 *
 * @param dep A post-expand Servicedependency.
 * @param poller_id The poller owning this dependency.
 */
void broker_cache::_insert_service_notif_dep(
    const engine::configuration::Servicedependency& dep,
    uint64_t poller_id) {
  if (dep.dependency_type() != engine::configuration::notification_dependency)
    return;

  const auto& by_name_idx = _services.get<by_name>();
  auto dependent = by_name_idx.find(
      std::make_pair(dep.dependent_hosts().data(0),
                     dep.dependent_service_description().data(0)));
  auto master = by_name_idx.find(
      std::make_pair(dep.hosts().data(0), dep.service_description().data(0)));
  if (dependent == by_name_idx.end() || master == by_name_idx.end()) {
    _logger->warn(
        "broker_cache: cannot store notification service dependency of "
        "'{}/{}' on '{}/{}' for poller {}: service not found in cache",
        dep.dependent_hosts().data(0),
        dep.dependent_service_description().data(0), dep.hosts().data(0),
        dep.service_description().data(0), poller_id);
    return;
  }
  _service_notif_deps.insert(service_notif_dep{
      (*dependent)->obj().host_id(), (*dependent)->obj().service_id(),
      (*master)->obj().host_id(), (*master)->obj().service_id(), poller_id,
      dep.dependency_period(), dep.inherits_parent(),
      dep.notification_failure_options(), servicedependency_key(dep)});
}

/**
 * @brief Store (or replace) a notification contact and register @p poller_id as
 * one of its referencing pollers.
 *
 * The contact is stored by name; only the fields the notification decision
 * needs are kept. Resolution of the contactgroups the contact belongs to is not
 * done here (it happens at notification time).
 *
 * @param c A configuration Contact (post-expand).
 * @param poller_id The poller referencing this contact.
 */
void broker_cache::_insert_contact(const engine::configuration::Contact& c,
                                   uint64_t poller_id) {
  const std::string& name = c.contact_name();
  contact entry;
  entry.name = name;
  entry.host_notifications_enabled = c.host_notifications_enabled();
  entry.service_notifications_enabled = c.service_notifications_enabled();
  entry.host_notification_period = c.host_notification_period();
  entry.service_notification_period = c.service_notification_period();
  entry.host_notification_options = c.host_notification_options();
  entry.service_notification_options = c.service_notification_options();
  entry.timezone = c.timezone();
  auto [it, inserted] = _contacts.try_emplace(name, std::move(entry),
                                              absl::flat_hash_set<uint64_t>{});
  if (!inserted)
    it->second.first = std::move(entry);
  it->second.second.insert(poller_id);
}

/**
 * @brief Store (or replace) a notification contactgroup and register
 * @p poller_id as one of its referencing pollers.
 *
 * The Engine flattens nested contactgroups into `members` before the
 * configuration reaches Broker, so `members` holds only concrete contact names.
 *
 * @param cg A configuration Contactgroup (post-expand).
 * @param poller_id The poller referencing this contactgroup.
 */
void broker_cache::_insert_contactgroup(
    const engine::configuration::Contactgroup& cg,
    uint64_t poller_id) {
  const std::string& name = cg.contactgroup_name();
  contactgroup entry;
  entry.name = name;
  for (const std::string& member : cg.members().data())
    entry.members.insert(member);
  auto [it, inserted] = _contactgroups.try_emplace(
      name, std::move(entry), absl::flat_hash_set<uint64_t>{});
  if (!inserted)
    it->second.first = std::move(entry);
  it->second.second.insert(poller_id);
}

/**
 * @brief Drop @p poller_id from every contact and contactgroup reference set,
 * erasing the entries no poller references anymore.
 *
 * Used both on a full re-merge (to rebuild the poller's contribution from
 * scratch) and on poller removal.
 *
 * @param poller_id The poller to drop.
 */
void broker_cache::_remove_poller_from_contacts(uint64_t poller_id) {
  std::vector<std::string> orphaned;
  for (auto& [name, value] : _contacts) {
    value.second.erase(poller_id);
    if (value.second.empty())
      orphaned.push_back(name);
  }
  for (const std::string& name : orphaned)
    _contacts.erase(name);

  orphaned.clear();
  for (auto& [name, value] : _contactgroups) {
    value.second.erase(poller_id);
    if (value.second.empty())
      orphaned.push_back(name);
  }
  for (const std::string& name : orphaned)
    _contactgroups.erase(name);
}

/**
 * @brief Apply a configuration state difference into the cache. Used when some
 * new Engine configurations are pushed by a user (i.e. when a
 * `pb_global_diff_state` event is received).
 *
 * Unlike `merge()`, the Host objects in the diff already carry their own
 * `poller_id` field, set by `indexed_diff_state::add_state()` before the
 * global diff is published. Therefore `_fill_host()` is called without a
 * `poller_id_hint` here — `cfg.poller_id()` is non-zero and is used directly.
 *
 * The group-link removal logic relies on `instance_id` (= poller_id) being
 * correctly set on the cached host objects: when a servicegroup or hostgroup
 * is removed from a poller, only the links whose host belongs to that poller
 * (identified via `_hosts.find(host_id)->obj().instance_id() ==
 * sgp/hgp.poller_id()`) are erased.
 *
 * @param diff Configuration state difference
 */
void broker_cache::apply(
    const com::centreon::engine::configuration::DiffState& diff) {
  absl::WriterMutexLock lck{&_mutex};

  _logger->debug("Applying configuration diff for poller id {} and name '{}'",
                 diff.poller_id(), diff.poller_name());

  /* A diff may toggle the program-wide notification flags or interval_length
   * without resending the whole state, so keep the cached values in sync. */
  if (section_enabled(CACHE_INSTANCES) &&
      (diff.has_enable_notifications() ||
       diff.has_send_recovery_notifications_anyway() ||
       diff.has_soft_state_dependencies() || diff.interval_length() > 0)) {
    auto it = _instances.find(diff.poller_id());
    if (it != _instances.end()) {
      if (diff.has_enable_notifications())
        it->second.notifications_enabled = diff.enable_notifications();
      if (diff.has_send_recovery_notifications_anyway())
        it->second.send_recovery_notifications_anyway =
            diff.send_recovery_notifications_anyway();
      if (diff.has_soft_state_dependencies())
        it->second.soft_state_dependencies = diff.soft_state_dependencies();
      /* interval_length is not optional in DiffState: 0 (not a valid value)
       * means "not part of the diff". */
      if (diff.interval_length() > 0)
        it->second.interval_length =
            std::chrono::seconds(diff.interval_length());
    }
  }

  /* Work on timeperiods. */
  if (section_enabled(CACHE_NOTIFICATIONS)) {
    const engine::configuration::DiffTimeperiod& dtp = diff.timeperiods();

    /* This condition is useful to avoid calling _resolve_timeperiods() when
     * nothing changed. */
    if (dtp.added_size() || dtp.modified_size() || dtp.removed_size()) {
      for (const engine::configuration::Timeperiod& tp : dtp.added()) {
        _timeperiods.insert_or_assign(
            tp.timeperiod_name(),
            std::make_shared<common::timeperiods::timeperiod>(tp, _logger));
        _timeperiod_pollers[tp.timeperiod_name()].insert(diff.poller_id());
      }
      for (const engine::configuration::Timeperiod& tp : dtp.modified()) {
        _timeperiods.insert_or_assign(
            tp.timeperiod_name(),
            std::make_shared<common::timeperiods::timeperiod>(tp, _logger));
        _timeperiod_pollers[tp.timeperiod_name()].insert(diff.poller_id());
      }
      for (const std::string& name : dtp.removed()) {
        auto it = _timeperiod_pollers.find(name);
        if (it != _timeperiod_pollers.end()) {
          it->second.erase(diff.poller_id());
          if (it->second.empty()) {
            _timeperiod_pollers.erase(it);
            _timeperiods.erase(name);
          }
        }
      }
      _resolve_timeperiods();
    }
  }

  /* Work on notification contacts and contactgroups. Same reference-counted
   * per-poller model as the timeperiods above; a DiffContact/DiffContactgroup
   * carries added, modified and removed (by name) entries. */
  if (section_enabled(CACHE_NOTIFICATIONS)) {
    const engine::configuration::DiffContact& dc = diff.contacts();
    for (const engine::configuration::Contact& c : dc.added())
      _insert_contact(c, diff.poller_id());
    for (const engine::configuration::Contact& c : dc.modified())
      _insert_contact(c, diff.poller_id());
    for (const std::string& name : dc.removed()) {
      auto it = _contacts.find(name);
      if (it != _contacts.end()) {
        it->second.second.erase(diff.poller_id());
        if (it->second.second.empty())
          _contacts.erase(it);
      }
    }

    const engine::configuration::DiffContactgroup& dcg = diff.contactgroups();
    for (const engine::configuration::Contactgroup& cg : dcg.added())
      _insert_contactgroup(cg, diff.poller_id());
    for (const engine::configuration::Contactgroup& cg : dcg.modified())
      _insert_contactgroup(cg, diff.poller_id());
    for (const std::string& name : dcg.removed()) {
      auto it = _contactgroups.find(name);
      if (it != _contactgroups.end()) {
        it->second.second.erase(diff.poller_id());
        if (it->second.second.empty())
          _contactgroups.erase(it);
      }
    }
  }

  /* Work on severities */
  if (section_enabled(CACHE_SEVERITIES)) {
    _logger->debug(
        "apply(): {} severities added {} severities modified {} "
        "severities removed",
        diff.severities().added_size(), diff.severities().modified_size(),
        diff.severities().removed_size());
    for (const engine::configuration::Severity& sev :
         diff.severities().added()) {
      auto key = std::make_pair(sev.key().id(), sev.key().type());
      auto it = _severities.find(key);
      if (it != _severities.end()) {
        it->second.first.level = sev.level();  // preserve existing db_id
        it->second.second.insert(sev.poller_id());
        _logger->debug(
            "apply() severity added (id={} type={} level={} poller_id={}): set "
            "now has {} pollers",
            sev.key().id(), sev.key().type(), sev.level(), sev.poller_id(),
            it->second.second.size());
      } else {
        _severities.insert({key,
                            {{sev.level(), 0},
                             absl::flat_hash_set<uint64_t>{sev.poller_id()}}});
        _logger->debug(
            "apply() severity added (id={} type={} level={} poller_id={}): new "
            "entry",
            sev.key().id(), sev.key().type(), sev.level(), sev.poller_id());
      }
    }
    for (const engine::configuration::Severity& sev :
         diff.severities().modified()) {
      auto key = std::make_pair(sev.key().id(), sev.key().type());
      auto it = _severities.find(key);
      if (it != _severities.end()) {
        it->second.first.level = sev.level();  // preserve existing db_id
        it->second.second.insert(sev.poller_id());
      } else {
        _severities.insert({key,
                            {{sev.level(), 0},
                             absl::flat_hash_set<uint64_t>{sev.poller_id()}}});
      }
    }
    for (const engine::configuration::SeverityKeyWithPoller& key :
         diff.severities().removed()) {
      auto sev_key = std::make_pair(key.id(), key.type());
      auto it = _severities.find(sev_key);
      if (it != _severities.end()) {
        auto before_size = it->second.second.size();
        it->second.second.erase(key.poller_id());
        auto after_size = it->second.second.size();
        _logger->debug(
            "apply() severity removed (id={} type={} poller_id={}): set "
            "size {} -> {}",
            key.id(), key.type(), key.poller_id(), before_size, after_size);
        if (it->second.second.empty())
          _severities.erase(it);
      } else {
        _logger->debug(
            "apply() severity removed (id={} type={} poller_id={}): NOT "
            "FOUND in cache",
            key.id(), key.type(), key.poller_id());
      }
    }
  }

  /* Removing hostgroups */
  if (section_enabled(CACHE_GROUPS)) {
    auto& hg_by_name = _hostgroups.get<by_name>();
    for (const auto& hgp : diff.hostgroups().removed()) {
      SPDLOG_LOGGER_DEBUG(_logger, "Removing hostgroup '{}' from poller {}",
                          hgp.group_name(), hgp.poller_id());
      if (auto hg = hg_by_name.find(hgp.group_name()); hg != hg_by_name.end()) {
        /* Removing a hostgroup is coming from a poller, but the hostgroup can
         * be shared between several pollers. So we can only break links
         * between hostgroup and hosts sharing the same poller. And if and
         * only if the hostgroup is empty, we can also remove it. */
        auto& host_by_hostgroup = _host_hostgroups.get<by_hostgroup>();
        auto [lower, upper] =
            host_by_hostgroup.equal_range(hg->first->obj().hostgroup_id());
        for (; lower != upper;) {
          auto hst_it = _hosts.find(lower->host_id);
          uint64_t poller_id = 0;
          if (hst_it != _hosts.end())
            poller_id = (*hst_it)->obj().instance_id();

          if (poller_id == hgp.poller_id())
            lower = host_by_hostgroup.erase(lower);
          else
            ++lower;
        }
        /* We can const_cast the pollers set because we don't change the
         * hostgroup itself, which is the key of the multiindex. */
        auto& set = const_cast<absl::flat_hash_set<uint64_t>&>(hg->second);
        set.erase(hgp.poller_id());
        if (set.empty()) {
          /* If no poller needs this hostgroup anymore, we can remove it from
           * the cache. */
          hg_by_name.erase(hg);
        }
      }
    }
  }

  /* Work on hosts */
  /* hosts_by_id is declared outside the guard because it is also needed by
   * the "Removing hosts" block later in this function. */
  auto& hosts_by_id = _hosts.get<by_id>();

  if (section_enabled(CACHE_HOSTS)) {
    /* Adding hosts */
    for (const engine::configuration::Host& host : diff.hosts().added()) {
      auto h = std::make_shared<neb::pb_host>();
      _fill_host(&h->mut_obj(), host);
      auto [it, inserted] = hosts_by_id.insert(h);
      if (!inserted)
        hosts_by_id.replace(it, h);
    }

    /* Modifying hosts */
    for (const engine::configuration::Host& host : diff.hosts().modified()) {
      auto h = std::make_shared<neb::pb_host>();
      _fill_host(&h->mut_obj(), host);
      auto [it, inserted] = hosts_by_id.insert(h);
      if (!inserted)
        hosts_by_id.replace(it, h);
    }
  }

  /* Work on hostgroups */
  auto feed_hostgroup = [&](const engine::configuration::Hostgroup& hg,
                            bool add) ABSL_NO_THREAD_SAFETY_ANALYSIS {
    auto& hg_index = _hostgroups.get<by_id>();
    auto found = hg_index.find(hg.hostgroup_id());
    bool inserted = false;
    if (found == hg_index.end()) {
      auto hostgroup = std::make_shared<neb::pb_host_group>();
      auto& obj = hostgroup->mut_obj();
      // fill_hostgroup(hostgroup->mut_obj(), hg);
      obj.set_hostgroup_id(hg.hostgroup_id());
      obj.set_name(hg.hostgroup_name());
      obj.set_enabled(true);
      /* Hostgroups are linked to several pollers, so we set poller_id to 0.
       */
      obj.set_poller_id(0);
      obj.set_alias(hg.alias());
      std::tie(found, inserted) = hg_index.emplace(
          hostgroup, absl::flat_hash_set<uint64_t>{hg.poller_id()});
    } else {
      /* The stored group is shared with readers that no longer hold the mutex:
       * clone it, change the clone, then swap. Doing it through
       * _replace_hostgroup() also keeps found valid, which an extract()
       * round-trip would not. The clone is skipped when the cached group
       * already carries the expected values, which is the common case when
       * many pollers declare the same groups. */
      auto current = found->first;
      if (current->obj().name() != hg.hostgroup_name() ||
          current->obj().alias() != hg.alias()) {
        auto hostgroup = std::make_shared<neb::pb_host_group>(*current);
        auto& obj = hostgroup->mut_obj();
        obj.set_name(hg.hostgroup_name());
        obj.set_alias(hg.alias());
        if (!_replace_hostgroup(found, hostgroup))
          SPDLOG_LOGGER_ERROR(
              _logger,
              "Failed to rename host group {} to '{}': another host group "
              "already uses this name",
              hg.hostgroup_id(), hg.hostgroup_name());
      }
      /* The poller set belongs to the container node and never escapes, so the
       * mutex alone protects it. */
      const_cast<absl::flat_hash_set<uint64_t>&>(found->second)
          .insert(hg.poller_id());
    }
    if (!add) {
      /* If it's not an addition, we have to remove the previous members of
       * this poller's contribution to the hostgroup from the cache, because
       * we don't know if they are still members or not. Members from other
       * pollers must be preserved. */
      auto& indexed_by_hostgroup = _host_hostgroups.get<by_hostgroup>();
      auto [lower, upper] =
          indexed_by_hostgroup.equal_range(found->first->obj().hostgroup_id());
      for (auto it = lower; it != upper;) {
        auto host_it = _hosts.find(it->host_id);
        uint64_t host_poller_id = 0;
        if (host_it != _hosts.end())
          host_poller_id = (*host_it)->obj().instance_id();
        if (host_poller_id == hg.poller_id())
          it = indexed_by_hostgroup.erase(it);
        else
          ++it;
      }
    }

    auto& index = _hosts.get<by_name>();
    for (const auto& member : hg.members().data()) {
      auto host_it = index.find(member);
      if (host_it == index.end())
        continue;

      uint64_t host_id = (*host_it)->obj().host_id();

      SPDLOG_LOGGER_DEBUG(_logger, "Linking host id {} to hostgroup id {}",
                          host_id, hg.hostgroup_id());
      _host_hostgroups.insert({host_id, found->first});
    }
  };

  if (section_enabled(CACHE_GROUPS)) {
    /* Adding hostgroups */
    for (const auto& hg : diff.hostgroups().added()) {
      SPDLOG_LOGGER_DEBUG(_logger, "Adding hostgroup '{}' (id {})",
                          hg.hostgroup_name(), hg.hostgroup_id());
      feed_hostgroup(hg, true);
    }

    /* Modifying hostgroups */
    for (const auto& hg : diff.hostgroups().modified()) {
      SPDLOG_LOGGER_DEBUG(_logger, "Modifying hostgroup '{}' (id {})",
                          hg.hostgroup_name(), hg.hostgroup_id());
      feed_hostgroup(hg, false);
    }
  }

  /* Removing servicegroups */
  if (section_enabled(CACHE_GROUPS)) {
    auto& sg_index = _servicegroups.get<by_name>();
    for (const auto& sgp : diff.servicegroups().removed()) {
      SPDLOG_LOGGER_DEBUG(_logger, "Removing servicegroup '{}' from poller {}",
                          sgp.group_name(), sgp.poller_id());
      if (auto sg_it = sg_index.find(sgp.group_name());
          sg_it != sg_index.end()) {
        /* If the servicegroup is still in the cache, we have to remove all
         * the members of the servicegroup from the cache, because we don't
         * know if they are still members or not. */
        auto& indexed_by_servicegroup =
            _service_servicegroups.get<by_servicegroup>();
        auto [lower, upper] = indexed_by_servicegroup.equal_range(
            sg_it->first->obj().servicegroup_id());
        uint64_t old_host_id = 0;
        uint64_t poller_id;
        for (; lower != upper;) {
          auto svc_it =
              _services.find(std::make_pair(lower->host_id, lower->service_id));
          if (svc_it != _services.end()) {
            /* Here comes the optimization, we only look for the poller_id if
             * the host_id changed, because services are ordered by host_id
             * and then by service_id. */
            if (lower->host_id != old_host_id) {
              old_host_id = lower->host_id;
              auto host_it = _hosts.find(lower->host_id);
              if (host_it != _hosts.end())
                poller_id = (*host_it)->obj().instance_id();
              else
                poller_id = 0;
            }
          } else
            poller_id = 0;

          if (poller_id == sgp.poller_id())
            lower = indexed_by_servicegroup.erase(lower);
          else
            ++lower;
        }
        auto& set = const_cast<absl::flat_hash_set<uint64_t>&>(sg_it->second);
        set.erase(sgp.poller_id());
        if (set.empty()) {
          /* If no poller needs this servicegroup anymore, we can remove it
           * from the cache. */
          sg_index.erase(sg_it);
        }
      }
    }
  }

  /* Work on services */
  if (section_enabled(CACHE_SERVICES)) {
    auto& s_index = _services.get<by_id>();

    /* Adding services */
    for (const engine::configuration::Service& svc : diff.services().added()) {
      auto s = std::make_shared<neb::pb_service>();
      _fill_service(&s->mut_obj(), svc);
      auto [it, inserted] = s_index.insert(s);
      if (!inserted)
        s_index.replace(it, s);
    }

    /* Modifying services */
    for (const engine::configuration::Service& svc :
         diff.services().modified()) {
      auto s = std::make_shared<neb::pb_service>();
      _fill_service(&s->mut_obj(), svc);
      auto [it, inserted] = s_index.insert(s);
      if (!inserted)
        s_index.replace(it, s);
    }

    /* Removing services */
    absl::flat_hash_set<uint64_t> removed_service_severity_ids;
    for (const auto& key : diff.services().removed()) {
      auto svc_it =
          s_index.find(std::make_pair(key.host_id(), key.service_id()));
      if (svc_it != s_index.end()) {
        uint64_t severity_id = (*svc_it)->obj().severity_id();
        if (severity_id)
          removed_service_severity_ids.insert(severity_id);
        s_index.erase(svc_it);
      }
    }

    /* Work on anomaly detections */
    /* Adding anomaly detections */
    for (const engine::configuration::Anomalydetection& ad :
         diff.anomalydetections().added()) {
      auto s = std::make_shared<neb::pb_service>();
      _fill_anomaly_detection(&s->mut_obj(), ad);
      auto [it, inserted] = s_index.insert(s);
      if (!inserted)
        s_index.replace(it, s);
      _anomaly_detection_index[std::make_pair(ad.host_id(),
                                              ad.dependent_service_id())]
          .insert(ad.service_id());
    }

    /* Modifying anomaly detections */
    for (const engine::configuration::Anomalydetection& ad :
         diff.anomalydetections().modified()) {
      auto old_it = s_index.find(std::make_pair(ad.host_id(), ad.service_id()));
      if (old_it != s_index.end()) {
        const auto& old_obj = (*old_it)->obj();
        auto& old_set = _anomaly_detection_index[std::make_pair(
            old_obj.host_id(), old_obj.dependent_service_id())];
        old_set.erase(old_obj.service_id());
        if (old_set.empty())
          _anomaly_detection_index.erase(std::make_pair(
              old_obj.host_id(), old_obj.dependent_service_id()));
      }
      auto s = std::make_shared<neb::pb_service>();
      _fill_anomaly_detection(&s->mut_obj(), ad);
      auto [it, inserted] = s_index.insert(s);
      if (!inserted)
        s_index.replace(it, s);
      _anomaly_detection_index[std::make_pair(ad.host_id(),
                                              ad.dependent_service_id())]
          .insert(ad.service_id());
    }

    /* Removing anomaly detections */
    for (const auto& key : diff.anomalydetections().removed()) {
      auto svc_it =
          s_index.find(std::make_pair(key.host_id(), key.service_id()));
      if (svc_it != s_index.end()) {
        const auto& obj = (*svc_it)->obj();
        auto& ad_set = _anomaly_detection_index[std::make_pair(
            obj.host_id(), obj.dependent_service_id())];
        ad_set.erase(obj.service_id());
        if (ad_set.empty())
          _anomaly_detection_index.erase(
              std::make_pair(obj.host_id(), obj.dependent_service_id()));
        uint64_t severity_id = obj.severity_id();
        if (severity_id)
          removed_service_severity_ids.insert(severity_id);
        s_index.erase(svc_it);
      }
    }

    /* Severities that are not used anymore by any service or anomaly
     * detection can be removed from the cache. */
    auto& index_by_severity = _services.get<by_severity>();
    for (uint64_t id : removed_service_severity_ids) {
      auto [lower, upper] = index_by_severity.equal_range(id);
      if (lower == upper) {
        /* If no service has this severity_id anymore, we can remove the
         * severity from the cache. */
        SPDLOG_LOGGER_DEBUG(
            _logger, "Removing severity id {} of type service from cache", id);
        _severities.erase(std::make_pair(id, Severity_Type_SERVICE));
        continue;
      }
    }
  }

  /* Work on servicegroups */
  auto feed_servicegroup = [&](const engine::configuration::Servicegroup& sg,
                               bool add) ABSL_NO_THREAD_SAFETY_ANALYSIS {
    auto& sg_index = _servicegroups.get<by_id>();
    auto found = sg_index.find(sg.servicegroup_id());
    if (found == sg_index.end()) {
      auto servicegroup = std::make_shared<neb::pb_service_group>();
      auto& obj = servicegroup->mut_obj();
      obj.set_servicegroup_id(sg.servicegroup_id());
      obj.set_name(sg.servicegroup_name());
      obj.set_enabled(true);
      /* In the cache, servicegrops are linked to several pollers, so we set
       * poller_id to 0. */
      obj.set_poller_id(0);
      obj.set_alias(sg.alias());
      bool inserted;
      std::tie(found, inserted) = sg_index.emplace(
          servicegroup, absl::flat_hash_set<uint64_t>{sg.poller_id()});
    } else {
      /* The stored group is shared with readers that no longer hold the mutex:
       * clone it, change the clone, then swap. Doing it through
       * _replace_servicegroup() also keeps found valid, which an extract()
       * round-trip would not. The clone is skipped when the cached group
       * already carries the expected values, which is the common case when
       * many pollers declare the same groups. */
      auto current = found->first;
      if (current->obj().name() != sg.servicegroup_name() ||
          current->obj().alias() != sg.alias()) {
        auto servicegroup = std::make_shared<neb::pb_service_group>(*current);
        auto& obj = servicegroup->mut_obj();
        obj.set_name(sg.servicegroup_name());
        obj.set_alias(sg.alias());
        if (!_replace_servicegroup(found, servicegroup))
          SPDLOG_LOGGER_ERROR(
              _logger,
              "Failed to rename service group {} to '{}': another service "
              "group already uses this name",
              sg.servicegroup_id(), sg.servicegroup_name());
      }
      /* The poller set belongs to the container node and never escapes, so the
       * mutex alone protects it. */
      const_cast<absl::flat_hash_set<uint64_t>&>(found->second)
          .insert(sg.poller_id());
    }
    if (!add) {
      /* If it's not an addition, we have to remove the previous members of
       * this poller's contribution to the servicegroup from the cache,
       * because we don't know if they are still members or not. Members from
       * other pollers must be preserved. */
      auto& indexed_by_servicegroup =
          _service_servicegroups.get<by_servicegroup>();
      auto [lower, upper] = indexed_by_servicegroup.equal_range(
          found->first->obj().servicegroup_id());
      uint64_t old_host_id = 0;
      uint64_t service_poller_id = 0;
      for (auto it = lower; it != upper;) {
        if (it->host_id != old_host_id) {
          old_host_id = it->host_id;
          auto host_it = _hosts.find(it->host_id);
          service_poller_id =
              (host_it != _hosts.end()) ? (*host_it)->obj().instance_id() : 0;
        }
        if (service_poller_id == sg.poller_id())
          it = indexed_by_servicegroup.erase(it);
        else
          ++it;
      }
    }

    auto& index = _services.get<by_name>();
    for (const auto& member : sg.members().data()) {
      auto service_it =
          index.find(std::make_pair(member.first(), member.second()));
      if (service_it == index.end())
        continue;

      uint64_t host_id = (*service_it)->obj().host_id();
      uint64_t service_id = (*service_it)->obj().service_id();
      SPDLOG_LOGGER_DEBUG(_logger,
                          "Linking service (host id {}, service id {}) "
                          "to servicegroup id {}",
                          host_id, service_id, sg.servicegroup_id());
      _service_servicegroups.insert({host_id, service_id, found->first});
    }
  };

  if (section_enabled(CACHE_GROUPS)) {
    /* Adding servicegroups */
    for (const auto& sg : diff.servicegroups().added()) {
      feed_servicegroup(sg, true);
    }

    /* Modifying servicegroups */
    for (const auto& sg : diff.servicegroups().modified()) {
      feed_servicegroup(sg, false);
    }
  }

  /* Removing hosts — deferred until after servicegroup processing so that
   * feed_servicegroup(!add) can still look up instance_id via _hosts. */
  if (section_enabled(CACHE_HOSTS)) {
    absl::flat_hash_set<uint64_t> removed_host_severity_ids;

    for (uint64_t host_id : diff.hosts().removed()) {
      auto host_it = hosts_by_id.find(host_id);
      if (host_it != hosts_by_id.end()) {
        auto& obj = (*host_it)->obj();
        uint64_t severity_id = obj.severity_id();
        if (severity_id)
          removed_host_severity_ids.insert(severity_id);

        hosts_by_id.erase(host_it);
      }
    }

    auto& index_by_severity = _hosts.get<by_severity>();
    for (uint64_t id : removed_host_severity_ids) {
      auto [lower, upper] = index_by_severity.equal_range(id);
      if (lower == upper) {
        /* If no host has this severity_id anymore, we can remove the severity
         * from the cache. */
        SPDLOG_LOGGER_DEBUG(
            _logger, "Removing severity id {} of type host from cache", id);
        _severities.erase(std::make_pair(id, Severity_Type_HOST));
        continue;
      }
    }
  }

  /* Work on tags */
  if (section_enabled(CACHE_TAGS)) {
    for (const engine::configuration::Tag& tag : diff.tags().added()) {
      auto pb = std::make_shared<neb::pb_tag>();
      auto& obj = pb->mut_obj();
      obj.set_id(tag.key().id());
      obj.set_type(static_cast<TagType>(tag.key().type()));
      obj.set_name(tag.tag_name());
      obj.set_action(Tag_Action_ADD);
      uint64_t pid = tag.poller_id();
      auto key = std::make_pair(obj.id(), obj.type());
      auto [it, inserted] = _tags.emplace(
          key, std::make_pair(pb, absl::flat_hash_set<uint64_t>{pid}));
      if (!inserted) {
        it->second.first = pb;
        it->second.second.insert(pid);
      }
    }
    for (const engine::configuration::Tag& tag : diff.tags().modified()) {
      auto pb = std::make_shared<neb::pb_tag>();
      auto& obj = pb->mut_obj();
      obj.set_id(tag.key().id());
      obj.set_type(static_cast<TagType>(tag.key().type()));
      obj.set_name(tag.tag_name());
      obj.set_action(Tag_Action_MODIFY);
      uint64_t pid = tag.poller_id();
      auto key = std::make_pair(obj.id(), obj.type());
      auto [it, inserted] = _tags.emplace(
          key, std::make_pair(pb, absl::flat_hash_set<uint64_t>{pid}));
      if (!inserted) {
        it->second.first = pb;
        it->second.second.insert(pid);
      }
    }
    for (const engine::configuration::TagKeyWithPoller& tp :
         diff.tags().removed()) {
      auto key = std::make_pair(tp.id(), static_cast<TagType>(tp.type()));
      auto it = _tags.find(key);
      if (it != _tags.end()) {
        it->second.second.erase(tp.poller_id());
        if (it->second.second.empty())
          _tags.erase(it);
      }
    }
  }

  /* Work on notification dependencies (incremental). The diff carries added
   * dependencies and a list of removed keys (the engine_conf *dependency_key()
   * hash). Because that key hashes every field, a content change surfaces as
   * added + removed; `modified` only appears on a hash collision, handled as a
   * replace (erase by key, then re-insert). Removals/replacements are matched
   * against the key stored on each cached entry, scoped to this poller. */
  if (section_enabled(CACHE_NOTIFICATIONS)) {
    const engine::configuration::DiffHostdependency& dhd =
        diff.hostdependencies();
    if (dhd.added_size() || dhd.modified_size() || dhd.removed_size()) {
      absl::flat_hash_set<size_t> to_erase;
      for (uint64_t k : dhd.removed())
        to_erase.insert(k);
      for (const engine::configuration::Hostdependency& dep : dhd.modified())
        to_erase.insert(hostdependency_key(dep));
      if (!to_erase.empty()) {
        auto& by_inst = _host_notif_deps.get<by_instance>();
        auto range = by_inst.equal_range(diff.poller_id());
        for (auto it = range.first; it != range.second;) {
          if (to_erase.contains(it->key))
            it = by_inst.erase(it);
          else
            ++it;
        }
      }
      for (const engine::configuration::Hostdependency& dep : dhd.added())
        _insert_host_notif_dep(dep, diff.poller_id());
      for (const engine::configuration::Hostdependency& dep : dhd.modified())
        _insert_host_notif_dep(dep, diff.poller_id());
    }
    const engine::configuration::DiffServicedependency& dsd =
        diff.servicedependencies();
    if (dsd.added_size() || dsd.modified_size() || dsd.removed_size()) {
      absl::flat_hash_set<size_t> to_erase;
      for (uint64_t k : dsd.removed())
        to_erase.insert(k);
      for (const engine::configuration::Servicedependency& dep : dsd.modified())
        to_erase.insert(servicedependency_key(dep));
      if (!to_erase.empty()) {
        auto& by_inst = _service_notif_deps.get<by_instance>();
        auto range = by_inst.equal_range(diff.poller_id());
        for (auto it = range.first; it != range.second;) {
          if (to_erase.contains(it->key))
            it = by_inst.erase(it);
          else
            ++it;
        }
      }
      for (const engine::configuration::Servicedependency& dep : dsd.added())
        _insert_service_notif_dep(dep, diff.poller_id());
      for (const engine::configuration::Servicedependency& dep : dsd.modified())
        _insert_service_notif_dep(dep, diff.poller_id());
    }
  }
}

/**
 * @brief We use BOOST_PP_SEQ_FOR_EACH to repeat setter without having to write
 * each of them.
 * Example:
 * BOOST_PP_SEQ_FOR_EACH(translate, ,(host_id)(acknowledged))
 * expands to:
 * ```code
 *   obj->set_host_id(in.host_id());
 *   obj->set_acknowledged(in.acknowledged());
 * ```
 */
#define set_proto(attrib) obj->set_##attrib(cfg.attrib());
#define translate(not_used_1, not_used_2, seq_head) set_proto(seq_head)

/**
 * @brief Fill a Host protobuf object from a configuration Host object.
 *
 * In centralized configuration mode, individual Host objects inside a
 * protobuf State message do not carry their own `poller_id` field — only the
 * enclosing State message has its `poller_id` set. The `poller_id_hint`
 * parameter is therefore used as a fallback when `cfg.poller_id()` is zero
 * (which is always the case in the `merge()` path). The resulting
 * `instance_id` field on the cached Host object is critical: it is used by
 * the group-link removal logic in `apply()` to identify which
 * service/host-group associations belong to a given poller and should be
 * cleaned up when that poller removes a group.
 *
 * @param obj             The protobuf Host object to fill
 * @param cfg             The configuration Host object to use as source
 * @param poller_id_hint  Fallback poller id used when cfg.poller_id() == 0.
 *                        Pass state.poller_id() from the enclosing State when
 *                        calling from merge(), or leave at 0 when the host
 *                        object already carries its own poller_id (apply()
 * path after indexed_diff_state sets it).
 */
void broker_cache::_fill_host(Host* obj,
                              const engine::configuration::Host& cfg,
                              uint64_t poller_id_hint) {
  uint64_t pid = cfg.poller_id() != 0 ? cfg.poller_id() : poller_id_hint;
  if (pid == 0) {
    SPDLOG_LOGGER_WARN(_logger,
                       "Host '{}' (id {}) has poller_id 0, which is not valid",
                       cfg.host_name(), cfg.host_id());
    return;
  }
  BOOST_PP_SEQ_FOR_EACH(
      translate, ,
      (host_id)(action_url)(address)(alias)(check_command)(check_freshness)(check_interval)(check_period)(display_name)(event_handler)(event_handler_enabled)(first_notification_delay)(recovery_notification_delay)(freshness_threshold)(high_flap_threshold)(icon_image)(icon_image_alt)(low_flap_threshold)(max_check_attempts)(notes)(notes_url)(notification_interval)(notification_period)(obsess_over_host)(retain_nonstatus_information)(retain_status_information)(retry_interval)(statusmap_image)(timezone)(severity_id)(icon_id));
  obj->set_name(cfg.host_name());
  obj->set_active_checks(cfg.checks_active());
  obj->set_passive_checks(cfg.checks_passive());
  obj->set_default_flap_detection(cfg.flap_detection_enabled());
  obj->set_flap_detection_on_down(
      cfg.flap_detection_options() &
      engine::configuration::ActionHostOn::action_hst_down);
  obj->set_flap_detection_on_up(
      cfg.flap_detection_options() &
      engine::configuration::ActionHostOn::action_hst_up);
  obj->set_flap_detection_on_unreachable(
      cfg.flap_detection_options() &
      engine::configuration::ActionHostOn::action_hst_unreachable);
  obj->set_notify_on_down(cfg.notification_options() &
                          engine::configuration::ActionHostOn::action_hst_down);
  obj->set_notify_on_recovery(
      cfg.notification_options() &
      engine::configuration::ActionHostOn::action_hst_up);
  obj->set_notify_on_unreachable(
      cfg.notification_options() &
      engine::configuration::ActionHostOn::action_hst_unreachable);
  obj->set_notify_on_flapping(
      cfg.notification_options() &
      engine::configuration::ActionHostOn::action_hst_flapping);
  obj->set_notify_on_downtime(
      cfg.notification_options() &
      engine::configuration::ActionHostOn::action_hst_downtime);
  obj->set_stalk_on_down(cfg.stalking_options() &
                         engine::configuration::ActionHostOn::action_hst_down);
  obj->set_stalk_on_up(cfg.stalking_options() &
                       engine::configuration::ActionHostOn::action_hst_up);
  obj->set_stalk_on_unreachable(
      cfg.stalking_options() &
      engine::configuration::ActionHostOn::action_hst_unreachable);
  for (const auto& tag : cfg.tags()) {
    auto* t = obj->add_tags();
    t->set_id(tag.first());
    t->set_type(static_cast<TagType>(tag.second()));
  }
  obj->set_instance_id(pid);
}

/**
 * @brief Fill common service fields from a configuration object.
 *
 * @tparam ConfigType The configuration type (pb_service or
 * pb_anomaly_detection configuration).
 * @param obj The Service protobuf object to fill.
 * @param cfg The source configuration object.
 */
template <typename ConfigType>
void broker_cache::_fill_service_common(Service* obj, const ConfigType& cfg) {
  BOOST_PP_SEQ_FOR_EACH(translate,
                        , (host_id)(service_id)(action_url)(check_freshness)(check_interval)(display_name)(event_handler)(first_notification_delay)(recovery_notification_delay)(freshness_threshold)(high_flap_threshold)(host_name)(icon_image)(icon_image_alt)(is_volatile)(low_flap_threshold)(max_check_attempts)(notes)(notes_url)(notification_interval)(notification_period)(obsess_over_service)(retain_nonstatus_information)(retain_status_information)(retry_interval)(timezone)(severity_id)(icon_id));
  obj->set_default_active_checks(cfg.checks_active());
  obj->set_default_passive_checks(cfg.checks_passive());
  obj->set_default_event_handler_enabled(cfg.event_handler_enabled());
  obj->set_default_flap_detection(cfg.flap_detection_enabled());
  obj->set_flap_detection_on_critical(
      cfg.flap_detection_options() &
      engine::configuration::ActionServiceOn::action_svc_critical);
  obj->set_flap_detection_on_ok(
      cfg.flap_detection_options() &
      engine::configuration::ActionServiceOn::action_svc_ok);
  obj->set_flap_detection_on_unknown(
      cfg.flap_detection_options() &
      engine::configuration::ActionServiceOn::action_svc_unknown);
  obj->set_flap_detection_on_warning(
      cfg.flap_detection_options() &
      engine::configuration::ActionServiceOn::action_svc_warning);
  obj->set_notify_on_critical(
      cfg.notification_options() &
      engine::configuration::ActionServiceOn::action_svc_critical);
  obj->set_notify_on_downtime(
      cfg.notification_options() &
      engine::configuration::ActionServiceOn::action_svc_downtime);
  obj->set_notify_on_flapping(
      cfg.notification_options() &
      engine::configuration::ActionServiceOn::action_svc_flapping);
  obj->set_notify_on_recovery(
      cfg.notification_options() &
      engine::configuration::ActionServiceOn::action_svc_ok);
  obj->set_notify_on_unknown(
      cfg.notification_options() &
      engine::configuration::ActionServiceOn::action_svc_unknown);
  obj->set_notify_on_warning(
      cfg.notification_options() &
      engine::configuration::ActionServiceOn::action_svc_warning);
  obj->set_stalk_on_critical(
      cfg.stalking_options() &
      engine::configuration::ActionServiceOn::action_svc_critical);
  obj->set_stalk_on_ok(cfg.stalking_options() &
                       engine::configuration::ActionServiceOn::action_svc_ok);
  obj->set_stalk_on_unknown(
      cfg.stalking_options() &
      engine::configuration::ActionServiceOn::action_svc_unknown);
  obj->set_stalk_on_warning(
      cfg.stalking_options() &
      engine::configuration::ActionServiceOn::action_svc_warning);

  for (const auto& tag : cfg.tags()) {
    auto* t = obj->add_tags();
    t->set_id(tag.first());
    t->set_type(static_cast<TagType>(tag.second()));
  }
  obj->set_description(cfg.service_description());
}

/**
 * @brief Fill a Service protobuf object from a configuration Service object.
 *
 * @param obj The protobuf Service object to fill
 * @param cfg The configuration Service object to use as source
 */
void broker_cache::_fill_service(Service* obj,
                                 const engine::configuration::Service& cfg) {
  _fill_service_common(obj, cfg);

  BOOST_PP_SEQ_FOR_EACH(translate, , (check_command)(check_period));

  uint32_t internal_id;
  if (absl::StartsWith(cfg.host_name(), "_Module_Meta") &&
      absl::StartsWith(cfg.service_description(), "meta_")) {
    obj->set_type(ServiceType::METASERVICE);
    std::string_view internal_id_str =
        absl::ClippedSubstr(cfg.service_description(), 5);
    if (absl::SimpleAtoi(internal_id_str, &internal_id))
      obj->set_internal_id(internal_id);
  } else if (absl::StartsWith(cfg.host_name(), "_Module_BAM") &&
             absl::StartsWith(cfg.service_description(), "ba_")) {
    obj->set_type(ServiceType::BA);
    std::string_view internal_id_str =
        absl::ClippedSubstr(cfg.service_description(), 3);
    if (absl::SimpleAtoi(internal_id_str, &internal_id))
      obj->set_internal_id(internal_id);
  } else
    obj->set_type(ServiceType::SERVICE);
}

/**
 * @brief Fill a Service protobuf object from a configuration Anomaly
 * detection object.
 *
 * @param obj The protobuf Service object to fill
 * @param cfg The configuration Service object to use as source
 */
void broker_cache::_fill_anomaly_detection(
    Service* obj,
    const engine::configuration::Anomalydetection& cfg) {
  _fill_service_common(obj, cfg);

  obj->set_type(ServiceType::ANOMALY_DETECTION);
  obj->set_dependent_service_id(cfg.dependent_service_id());
}

#undef translate
#undef set_proto

/**
 * @brief Update an instance in the cache.
 *
 * @param instance The instance to update
 */
void broker_cache::update_instance(
    const std::shared_ptr<neb::pb_instance>& instance) {
  if (!section_enabled(CACHE_INSTANCES))
    return;
  absl::WriterMutexLock l{&_mutex};
  auto& obj = instance->obj();
  if (obj.running()) {
    /* The neb Instance event carries none of the poller settings
     * (notifications_enabled, interval_length...), so only refresh the name
     * and keep the values already set by merge()/apply(). */
    _instances[obj.instance_id()].name = obj.name();
  } else
    _instances.erase(obj.instance_id());
}

/**
 * @brief Remove an instance from the cache.
 *
 * @param instance_id The ID of the instance to remove
 */
void broker_cache::remove_instance(uint64_t instance_id) {
  absl::WriterMutexLock l{&_mutex};

  /* Drop this poller's notification data (timeperiods and dependencies).
   * Timeperiods are reference-counted per poller: erase those no longer
   * referenced by any poller. Both are only populated when CACHE_NOTIFICATIONS
   * is enabled (see merge()/apply()). */
  if (section_enabled(CACHE_NOTIFICATIONS)) {
    std::vector<std::string> orphaned_timeperiods;
    for (auto& [name, pollers] : _timeperiod_pollers) {
      pollers.erase(instance_id);
      if (pollers.empty())
        orphaned_timeperiods.push_back(name);
    }
    for (const std::string& name : orphaned_timeperiods) {
      _timeperiod_pollers.erase(name);
      _timeperiods.erase(name);
    }

    _host_notif_deps.get<by_instance>().erase(instance_id);
    _service_notif_deps.get<by_instance>().erase(instance_id);

    /* Contacts/contactgroups are reference-counted per poller too. */
    _remove_poller_from_contacts(instance_id);
  }

  if (!section_enabled(CACHE_INSTANCES | CACHE_HOSTS | CACHE_SERVICES))
    return;
  _instances.erase(instance_id);

  auto& index_svc = _services.get<by_id>();
  auto& host_by_instance = _hosts.get<by_instance>();
  auto range = host_by_instance.equal_range(instance_id);
  for (auto it = range.first; it != range.second;) {
    auto lower =
        index_svc.lower_bound(std::make_pair((*it)->obj().host_id(), 0));
    auto upper =
        index_svc.upper_bound(std::make_pair((*it)->obj().host_id() + 1, 0));
    for (auto svc_it = lower; svc_it != upper; ++svc_it) {
      const auto& obj = (*svc_it)->obj();
      if (obj.type() == ServiceType::ANOMALY_DETECTION) {
        auto& ad_set = _anomaly_detection_index[std::make_pair(
            obj.host_id(), obj.dependent_service_id())];
        ad_set.erase(obj.service_id());
        if (ad_set.empty())
          _anomaly_detection_index.erase(
              std::make_pair(obj.host_id(), obj.dependent_service_id()));
      }
    }
    index_svc.erase(lower, upper);
    it = host_by_instance.erase(it);
  }
}

/**
 * @brief Update a servicegroup in the cache.
 *
 * @param servicegroup The servicegroup to update
 */
void broker_cache::update_servicegroup(
    const std::shared_ptr<neb::pb_service_group>& servicegroup) {
  if (!section_enabled(CACHE_GROUPS))
    return;
  absl::WriterMutexLock l{&_mutex};

  const auto& sg_id = servicegroup->obj().servicegroup_id();
  const auto& poller_id = servicegroup->obj().poller_id();

  assert(poller_id > 0);

  SPDLOG_LOGGER_DEBUG(
      _logger,
      "Updating service group '{}' (id {}) for poller {} in Broker cache.",
      servicegroup->obj().name(), sg_id, poller_id);

  if (servicegroup->obj().enabled()) {
    auto& sg_index = _servicegroups.get<by_id>();
    if (auto found = sg_index.find(sg_id); found != sg_index.end()) {
      /* The element already exists, we update it. The stored group is shared
       * with readers that no longer hold the mutex: clone it, change the clone,
       * then swap. Nothing to do when the cached group already carries the
       * expected values. */
      auto current = found->first;
      if (current->obj().name() != servicegroup->obj().name() ||
          current->obj().alias() != servicegroup->obj().alias()) {
        auto sg = std::make_shared<neb::pb_service_group>(*current);
        auto& obj = sg->mut_obj();
        obj.set_name(servicegroup->obj().name());
        obj.set_alias(servicegroup->obj().alias());
        if (!_replace_servicegroup(found, sg))
          SPDLOG_LOGGER_ERROR(
              _logger,
              "Failed to rename service group {} to '{}': another service "
              "group already uses this name",
              sg_id, servicegroup->obj().name());
      }
      /* The poller set belongs to the container node and never escapes, so the
       * mutex alone protects it. */
      const_cast<absl::flat_hash_set<uint64_t>&>(found->second)
          .insert(poller_id);
    } else {
      // The element is missing, we create it and insert it
      auto filled_servicegroup = std::make_shared<neb::pb_service_group>();
      ServiceGroup& obj = filled_servicegroup->mut_obj();
      obj.set_servicegroup_id(servicegroup->obj().servicegroup_id());
      obj.set_name(servicegroup->obj().name());
      obj.set_enabled(servicegroup->obj().enabled());
      obj.set_poller_id(servicegroup->obj().poller_id());
      obj.set_alias(servicegroup->obj().alias());
      _servicegroups.emplace(filled_servicegroup,
                             absl::flat_hash_set<uint64_t>{poller_id});
    }
  } else if (auto found = _servicegroups.get<by_id>().find(sg_id);
             found != _servicegroups.end()) {
    const_cast<absl::flat_hash_set<uint64_t>&>(found->second).erase(poller_id);
    if (found->second.empty())
      _servicegroups.erase(found);
  }
}

/**
 * @brief Update a hostgroup in the cache.
 *
 * @param hostgroup The hostgroup to update
 */
void broker_cache::update_hostgroup(
    const std::shared_ptr<neb::pb_host_group>& hostgroup) {
  if (!section_enabled(CACHE_GROUPS))
    return;
  absl::WriterMutexLock l{&_mutex};

  const auto& hg_id = hostgroup->obj().hostgroup_id();
  const auto& poller_id = hostgroup->obj().poller_id();

  assert(poller_id > 0);

  SPDLOG_LOGGER_DEBUG(
      _logger,
      "Updating host group '{}' (id {}) for poller {} in Broker cache.",
      hostgroup->obj().name(), hg_id, poller_id);

  if (hostgroup->obj().enabled()) {
    auto& hg_index = _hostgroups.get<by_id>();
    if (auto found = hg_index.find(hg_id); found != hg_index.end()) {
      // The element already exists, we update it
      SPDLOG_LOGGER_DEBUG(
          _logger,
          "Host group '{}' (id {}) already exists in Broker cache, updating "
          "it.",
          hostgroup->obj().name(), hg_id);
      auto& pollers = const_cast<absl::flat_hash_set<uint64_t>&>(found->second);
      pollers.insert(poller_id);
      /* The stored group is shared with readers that no longer hold the mutex:
       * clone it, change the clone, then swap. Nothing to do when the cached
       * group already carries the expected values. */
      auto current = found->first;
      if (current->obj().name() != hostgroup->obj().name() ||
          current->obj().alias() != hostgroup->obj().alias() ||
          current->obj().enabled() != hostgroup->obj().enabled() ||
          current->obj().poller_id() != 0) {
        auto hg = std::make_shared<neb::pb_host_group>(*current);
        HostGroup& obj = hg->mut_obj();
        obj.set_hostgroup_id(hostgroup->obj().hostgroup_id());
        obj.set_name(hostgroup->obj().name());
        obj.set_enabled(hostgroup->obj().enabled());
        /* The poller ID is not updated because a hostgroup can be linked to
         * several pollers, so no sense to keep it or replace it. */
        obj.set_poller_id(0);
        obj.set_alias(hostgroup->obj().alias());
        if (!_replace_hostgroup(found, hg))
          SPDLOG_LOGGER_ERROR(_logger,
                              "Failed to rename host group {} to '{}': another "
                              "host group already uses this name",
                              hg_id, hostgroup->obj().name());
      }
    } else {
      // The element is missing, we create it and insert it
      SPDLOG_LOGGER_DEBUG(
          _logger,
          "Host group '{}' (id {}) does not exist in Broker cache, adding "
          "it.",
          hostgroup->obj().name(), hg_id);
      auto filled_hostgroup = std::make_shared<neb::pb_host_group>();
      HostGroup& obj = filled_hostgroup->mut_obj();
      obj.set_hostgroup_id(hostgroup->obj().hostgroup_id());
      obj.set_name(hostgroup->obj().name());
      obj.set_enabled(hostgroup->obj().enabled());
      /* The poller ID is not set because a hostgroup can be linked to several
       * pollers, so no sense to keep it or replace it. */
      obj.set_poller_id(0);
      obj.set_alias(hostgroup->obj().alias());
      _hostgroups.emplace(filled_hostgroup,
                          absl::flat_hash_set<uint64_t>{poller_id});
    }
  } else if (auto found = _hostgroups.get<by_id>().find(hg_id);
             found != _hostgroups.end()) {
    SPDLOG_LOGGER_DEBUG(
        _logger,
        "Host group '{}' (id {}) is disabled for poller {}. Removing it from "
        "Broker cache.",
        hostgroup->obj().name(), hg_id, poller_id);
    const_cast<absl::flat_hash_set<uint64_t>&>(found->second).erase(poller_id);
    if (found->second.empty()) {
      SPDLOG_LOGGER_INFO(_logger,
                         "Removing host group '{}' (id {}) from Broker cache.",
                         found->first->obj().name(), hg_id);
      _hostgroups.erase(found);
    }
  }
}

/**
 * @brief Update a hostgroup member in the cache.
 *
 * @param hostgroup_member The hostgroup member to update
 */
void broker_cache::update_hostgroup_member(
    const std::shared_ptr<neb::pb_host_group_member>& hostgroup_member) {
  if (!section_enabled(CACHE_GROUPS))
    return;
  absl::WriterMutexLock l{&_mutex};

  const auto& hgm_obj = hostgroup_member->obj();
  uint64_t poller_id = hgm_obj.poller_id();
  assert(poller_id > 0);

  SPDLOG_LOGGER_DEBUG(_logger,
                      "Processing host group member (group_name: '{}', "
                      "group_id: {}, host_id: "
                      "{}, enabled: {})",
                      hgm_obj.name(), hgm_obj.hostgroup_id(), hgm_obj.host_id(),
                      hgm_obj.enabled());
  auto key = std::make_pair(hgm_obj.host_id(), hgm_obj.hostgroup_id());

  if (hgm_obj.enabled()) {
    auto found = _hostgroups.get<by_id>().find(hgm_obj.hostgroup_id());
    bool inserted;
    if (found == _hostgroups.get<by_id>().end()) {
      auto hg = std::make_shared<neb::pb_host_group>();
      auto& obj = hg->mut_obj();
      obj.set_hostgroup_id(hgm_obj.hostgroup_id());
      obj.set_name(hgm_obj.name());
      obj.set_enabled(true);
      /* The poller ID is not set because a hostgroup can be linked to several
       * pollers, so no sense to keep it or replace it. */
      obj.set_poller_id(0);
      std::tie(found, inserted) =
          _hostgroups.insert({hg, absl::flat_hash_set<uint64_t>{poller_id}});
    }
    _host_hostgroups.insert({hgm_obj.host_id(), found->first});

    if (found->first->obj().name() != hgm_obj.name()) {
      /* The stored group is shared with readers that no longer hold the mutex:
       * clone it, change the clone, then swap. Going through
       * _replace_hostgroup() also re-keys the by_name index, which an in-place
       * rename would silently corrupt, and restores the previous name when
       * another group already uses the new one. */
      auto hg = std::make_shared<neb::pb_host_group>(*found->first);
      hg->mut_obj().set_name(hgm_obj.name());
      if (!_replace_hostgroup(found, hg))
        SPDLOG_LOGGER_ERROR(
            _logger, "Failed to update the name of the host group {} to '{}'",
            hgm_obj.hostgroup_id(), hgm_obj.name());
    }
  } else {
    _host_hostgroups.erase(key);
  }
}

/**
 * @brief Update a servicegroup member in the cache.
 *
 * @param servicegroup_member The servicegroup member to update
 */
void broker_cache::update_servicegroup_member(
    const std::shared_ptr<neb::pb_service_group_member>& servicegroup_member) {
  if (!section_enabled(CACHE_GROUPS))
    return;
  absl::WriterMutexLock l{&_mutex};

  const auto& sgm_obj = servicegroup_member->obj();
  assert(sgm_obj.poller_id() > 0 && sgm_obj.host_id() > 0);

  SPDLOG_LOGGER_DEBUG(_logger,
                      "Processing service group member (group_name: '{}', "
                      "group_id: {}, service_id: "
                      "{}, enabled: {})",
                      sgm_obj.name(), sgm_obj.servicegroup_id(),
                      sgm_obj.service_id(), sgm_obj.enabled());
  auto key = std::make_tuple(sgm_obj.host_id(), sgm_obj.service_id(),
                             sgm_obj.servicegroup_id());

  if (sgm_obj.enabled()) {
    auto found = _servicegroups.get<by_id>().find(sgm_obj.servicegroup_id());
    bool inserted;
    if (found == _servicegroups.get<by_id>().end()) {
      auto sg = std::make_shared<neb::pb_service_group>();
      auto& obj = sg->mut_obj();
      obj.set_servicegroup_id(sgm_obj.servicegroup_id());
      obj.set_name(sgm_obj.name());
      obj.set_enabled(true);
      /* The poller ID is not set because a servicegroup can be linked to
       * several pollers, so no sense to keep it or replace it. */
      obj.set_poller_id(0);
      std::tie(found, inserted) = _servicegroups.insert(
          {sg, absl::flat_hash_set<uint64_t>{sgm_obj.poller_id()}});
    }
    _service_servicegroups.insert(
        {sgm_obj.host_id(), sgm_obj.service_id(), found->first});
    if (found->first->obj().name() != sgm_obj.name()) {
      /* The stored group is shared with readers that no longer hold the mutex:
       * clone it, change the clone, then swap. Going through
       * _replace_servicegroup() also re-keys the by_name index, which an
       * in-place rename would silently corrupt, and restores the previous name
       * when another group already uses the new one. */
      auto sg = std::make_shared<neb::pb_service_group>(*found->first);
      sg->mut_obj().set_name(sgm_obj.name());
      if (!_replace_servicegroup(found, sg))
        SPDLOG_LOGGER_ERROR(
            _logger,
            "Failed to update the name of the service group {} to '{}'",
            sgm_obj.servicegroup_id(), sgm_obj.name());
    }
  } else {
    _service_servicegroups.erase(key);
  }
}

/**
 * @brief Update a metric mapping in the cache.
 *
 * @param mm The metric mapping to update.
 */
void broker_cache::update_metric_mapping(
    const std::shared_ptr<storage::pb_metric_mapping>& mm) {
  if (!section_enabled(CACHE_METRIC_MAPPINGS))
    return;
  absl::WriterMutexLock l{&_mutex};
  _metric_mappings[mm->obj().metric_id()] = mm;
}

/**
 * @brief Add a host to the cache (used in legacy mode).
 *
 * @param host The host to add
 */
void broker_cache::update_host(const std::shared_ptr<neb::pb_host>& host) {
  if (!section_enabled(CACHE_HOSTS))
    return;
  absl::WriterMutexLock l{&_mutex};
  auto& index = _hosts.get<by_id>();
  auto& h = host->obj();
  SPDLOG_LOGGER_DEBUG(_logger, "Processing host '{}' of id {} enabled {}",
                      h.name(), h.host_id(), h.enabled());
  auto it = index.find(h.host_id());
  if (h.enabled()) {
    if (it != index.end())
      index.replace(it, host);
    else
      index.insert(host);
  } else {
    if (it != index.end())
      index.erase(it);
  }
}

/**
 * @brief Update a host in the cache.
 *
 * @param status The host status used to update the host.
 */
void broker_cache::update_host(
    const std::shared_ptr<neb::pb_host_status>& status) {
  if (!section_enabled(CACHE_HOSTS))
    return;

  auto& hs = status->obj();
  uint64_t host_id = hs.host_id();
  bool updated = false;
  std::shared_ptr<neb::pb_acknowledgement> ack_to_close;
  {
    absl::WriterMutexLock l{&_mutex};
    auto& index = _hosts.get<by_id>();
    auto found = index.find(host_id);
    if (found != index.end()) {
      /* The cached host is shared with readers that no longer hold the mutex:
       * clone it, change the clone, then swap. */
      auto h = std::make_shared<neb::pb_host>(*found->get());
      auto& hst = h->mut_obj();
      hst.set_checked(hs.checked());
      hst.set_check_type(static_cast<Host_CheckType>(hs.check_type()));
      hst.set_state(static_cast<Host_State>(hs.state()));
      hst.set_state_type(static_cast<Host_StateType>(hs.state_type()));
      hst.set_last_state_change(hs.last_state_change());
      hst.set_last_hard_state(static_cast<Host_State>(hs.last_hard_state()));
      hst.set_last_hard_state_change(hs.last_hard_state_change());
      hst.set_last_time_up(hs.last_time_up());
      hst.set_last_time_down(hs.last_time_down());
      hst.set_last_time_unreachable(hs.last_time_unreachable());
      hst.set_output(hs.output());
      hst.set_perfdata(hs.perfdata());
      hst.set_flapping(hs.flapping());
      hst.set_percent_state_change(hs.percent_state_change());
      hst.set_latency(hs.latency());
      hst.set_execution_time(hs.execution_time());
      hst.set_last_check(hs.last_check());
      hst.set_next_check(hs.next_check());
      hst.set_should_be_scheduled(hs.should_be_scheduled());
      hst.set_check_attempt(hs.check_attempt());
      hst.set_notification_number(hs.notification_number());
      hst.set_no_more_notifications(hs.no_more_notifications());
      hst.set_last_notification(hs.last_notification());
      hst.set_next_host_notification(hs.next_host_notification());
      hst.set_acknowledgement_type(hs.acknowledgement_type());
      /* When Broker owns downtime management, the cached depth is authoritative
       * (maintained by broker_downtime_callbacks); don't let an Engine status
       * overwrite it. */
      if (!com::centreon::common::downtimes::downtime_manager::is_loaded())
        hst.set_scheduled_downtime_depth(hs.scheduled_downtime_depth());
      index.replace(found, h);
      updated = true;

      /* Acknowledgement event: decide under the lock, publish after release. */
      ack_to_close =
          _take_expired_acknowledgement(host_id, 0u, hst.acknowledgement_type(),
                                        static_cast<uint16_t>(hst.state()));
    } else {
      SPDLOG_LOGGER_WARN(_logger,
                         "Attempt to update host ({}) in Broker cache, but "
                         "it does not exist.",
                         host_id);
    }
  }

  if (ack_to_close) {
    multiplexing::publisher pblshr;
    pblshr.write(std::move(ack_to_close));
  }

  if (updated)
    SPDLOG_LOGGER_DEBUG(
        _logger, "Updated host status for host '{}' in Broker cache.", host_id);
  else
    SPDLOG_LOGGER_DEBUG(
        _logger,
        "Host status for host '{}' was not updated in Broker cache because "
        "host does not exist in cache.",
        host_id);
}

/**
 * @brief Update a host in the cache.
 *
 * @param host The adaptive host used to update the host.
 */
void broker_cache::update_host(
    const std::shared_ptr<neb::pb_adaptive_host>& host) {
  if (!section_enabled(CACHE_HOSTS))
    return;
  absl::WriterMutexLock l{&_mutex};
  auto& ah = host->obj();
  auto& index = _hosts.get<by_id>();
  auto found = index.find(ah.host_id());
  if (found != index.end()) {
    /* The cached host is shared with readers that no longer hold the mutex:
     * clone it, change the clone, then swap. */
    auto hp = std::make_shared<neb::pb_host>(*found->get());
    auto& h = hp->mut_obj();
    SPDLOG_LOGGER_DEBUG(_logger,
                        "Updating adaptive host for host '{}' in Broker cache.",
                        ah.host_id());
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
    index.replace(found, hp);
  } else
    SPDLOG_LOGGER_WARN(
        _logger,
        "Cannot update cache for host {}, it does not exist in the cache",
        ah.host_id());
}

/**
 * @brief Update a host in the cache.
 *
 * @param status The adaptive host status used to update the host.
 */
void broker_cache::update_host(
    const std::shared_ptr<neb::pb_adaptive_host_status>& status) {
  if (!section_enabled(CACHE_HOSTS))
    return;

  std::shared_ptr<neb::pb_acknowledgement> ack_to_close;
  {
    absl::WriterMutexLock l{&_mutex};
    auto& hs = status->obj();
    auto& index = _hosts.get<by_id>();
    auto found = index.find(hs.host_id());
    if (found != index.end()) {
      auto h = std::make_shared<neb::pb_host>(*found->get());
      auto& hst = h->mut_obj();
      SPDLOG_LOGGER_DEBUG(
          _logger,
          "Updating adaptive host status for host '{}' in Broker cache.",
          hs.host_id());
      if (hs.has_scheduled_downtime_depth())
        hst.set_scheduled_downtime_depth(hs.scheduled_downtime_depth());
      if (hs.has_acknowledgement_type())
        hst.set_acknowledgement_type(hs.acknowledgement_type());
      if (hs.has_notification_number())
        hst.set_notification_number(hs.notification_number());

      /* Acknowledgement event: decide under the lock, publish after release. */
      ack_to_close = _take_expired_acknowledgement(
          hs.host_id(), 0u, hst.acknowledgement_type(),
          static_cast<uint16_t>(hst.state()));
      index.replace(found, h);
    } else {
      SPDLOG_LOGGER_WARN(
          _logger,
          "Attempt to update host ({}) in Broker cache, but it does not exist.",
          hs.host_id());
    }
  }
  if (ack_to_close) {
    multiplexing::publisher pblshr;
    pblshr.write(std::move(ack_to_close));
  }
}

/**
 * @brief Add a service to the cache (used in legacy mode).
 *
 * @param svc The service to add
 */
void broker_cache::update_service(const std::shared_ptr<neb::pb_service>& svc) {
  if (!section_enabled(CACHE_SERVICES))
    return;
  absl::WriterMutexLock l{&_mutex};

  auto& index = _services.get<by_id>();
  auto& s = svc->obj();
  SPDLOG_LOGGER_DEBUG(
      _logger, "Processing service ({}, {}) (description:{}) enabled {}",
      s.host_id(), s.service_id(), s.description(), s.enabled());

  auto it = index.find(std::make_pair(s.host_id(), s.service_id()));
  if (s.enabled()) {
    if (it != index.end())
      index.replace(it, svc);
    else
      index.insert(svc);
  } else {
    if (it != index.end())
      index.erase(it);
  }
}

/**
 * @brief Update a service in the cache.
 *
 * @param status The service status used to update the service.
 */
void broker_cache::update_service(
    const std::shared_ptr<neb::pb_service_status>& status) {
  if (!section_enabled(CACHE_SERVICES))
    return;

  std::shared_ptr<neb::pb_acknowledgement> ack_to_close;
  {
    absl::WriterMutexLock l{&_mutex};

    const auto& obj = status->obj();
    SPDLOG_LOGGER_DEBUG(_logger, "Processing service status ({}, {})",
                        obj.host_id(), obj.service_id());

    auto& index = _services.get<by_id>();
    auto it = index.find(std::make_pair(obj.host_id(), obj.service_id()));
    if (it == index.end()) {
      SPDLOG_LOGGER_WARN(
          _logger,
          "Attempt to update service ({}, {}) in cache, but it does not exist.",
          obj.host_id(), obj.service_id());
      return;
    }

    /* The cached service is shared with readers that no longer hold the mutex:
     * clone it, change the clone, then swap. */
    auto s = std::make_shared<neb::pb_service>(*it->get());
    auto& svc = s->mut_obj();

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
    /* When Broker owns downtime management, the cached scheduled_downtime_depth
     * is authoritative (maintained by broker_downtime_callbacks). A status from
     * Engine must not overwrite it. */
    if (!com::centreon::common::downtimes::downtime_manager::is_loaded())
      svc.set_scheduled_downtime_depth(obj.scheduled_downtime_depth());
    index.replace(it, s);

    /* Acknowledgement event: decide under the lock, publish after release. */
    ack_to_close = _take_expired_acknowledgement(
        obj.host_id(), obj.service_id(), svc.acknowledgement_type(),
        static_cast<uint16_t>(svc.state()));
  }
  if (ack_to_close) {
    multiplexing::publisher pblshr;
    pblshr.write(std::move(ack_to_close));
  }
}

/**
 * @brief Insert or remove an acknowledgement in the cache from an
 * acknowledgement event flowing through the multiplexer.
 *
 * Gated on the section matching the acked resource (CACHE_HOSTS for a host ack,
 * CACHE_SERVICES for a service ack). An open acknowledgement (deletion_time
 * == 0) is stored; a closing one (deletion_time > 0) removes the entry, so the
 * closure event republished by an expiration is not re-ingested as a new ack.
 *
 * @param ack The acknowledgement event (host acks carry service_id 0).
 */
void broker_cache::update_acknowledgement(
    const std::shared_ptr<neb::pb_acknowledgement>& ack) {
  const auto& obj = ack->obj();
  /* Gate on the section matching the acked resource: a host ack (service_id 0)
   * is only relevant when CACHE_HOSTS is enabled, a service ack when
   * CACHE_SERVICES is. This MUST match the gating of the update_host() /
   * update_service() expiration paths, otherwise an ack could be stored but
   * never expired (or looked up but never stored). */
  if (!section_enabled(obj.service_id() == 0 ? CACHE_HOSTS : CACHE_SERVICES))
    return;
  absl::WriterMutexLock l{&_mutex};
  /* A closing acknowledgement (deletion_time set) must not repopulate the map:
   * the closure published by _take_expired_acknowledgement()'s caller is fed
   * back here through the multiplexer, so treat it as a removal, not an
   * insertion. Open acks have deletion_time == 0 on both the native and the
   * bbdo2-converted paths. */
  if (obj.deletion_time() > 0)
    _acknowledgements.erase({obj.host_id(), obj.service_id()});
  else
    _acknowledgements.insert_or_assign({obj.host_id(), obj.service_id()}, ack);
}

/**
 * @brief Return a snapshot of the acknowledgements currently held in the cache.
 *
 * @return A vector with one shared_ptr per cached acknowledgement.
 */
std::vector<std::shared_ptr<neb::pb_acknowledgement>>
broker_cache::acknowledgements() const {
  absl::ReaderMutexLock lck{&_mutex};
  std::vector<std::shared_ptr<neb::pb_acknowledgement>> result;
  result.reserve(_acknowledgements.size());
  for (const auto& it : _acknowledgements)
    result.push_back(it.second);
  return result;
}

/**
 * @brief Detect and remove an expired acknowledgement for a resource.
 *
 * Must be called with _mutex held. When the resource is no longer acknowledged
 * (@p ack_type == NONE) and a matching ack is cached, the entry is removed from
 * _acknowledgements; if the ack must be closed (it did not merely expire on
 * recovery), its deletion_time is stamped and the ack is returned so the CALLER
 * publishes it AFTER releasing _mutex. Publishing under the cache lock would
 * feed the event back into _publish()/update_acknowledgement() and risk a
 * re-entrant / lock-order deadlock with the multiplexer.
 *
 * @param host_id The host id.
 * @param service_id The service id, or 0 for a host acknowledgement.
 * @param ack_type The resource's current acknowledgement type.
 * @param state The resource's current state.
 * @return The acknowledgement to publish (deletion_time set), or nullptr when
 * there is nothing to publish.
 */
std::shared_ptr<neb::pb_acknowledgement>
broker_cache::_take_expired_acknowledgement(uint64_t host_id,
                                            uint64_t service_id,
                                            AckType ack_type,
                                            uint16_t state) {
  if (ack_type != AckType::NONE)
    return nullptr;
  SPDLOG_LOGGER_DEBUG(_logger, "Looking for acknowledgement on ({}:{})",
                      host_id, service_id);
  auto it = _acknowledgements.find({host_id, service_id});
  if (it == _acknowledgements.end())
    return nullptr;

  auto ack = it->second;
  _acknowledgements.erase(it);
  SPDLOG_LOGGER_DEBUG(_logger, "acknowledgement found on ({}:{})", host_id,
                      service_id);
  auto& ack_obj = ack->mut_obj();
  /* Close the ack (and ask the caller to publish it) unless the resource just
   * recovered (state OK) or a non-sticky ack moved to a different state. */
  if (!(!state  // !(OK or (normal ack and NOK))
        || (!ack_obj.sticky() && state != ack_obj.state()))) {
    SPDLOG_LOGGER_DEBUG(_logger,
                        "Set deletion time for acknowledgement on ({}:{})",
                        host_id, service_id);
    ack_obj.set_deletion_time(time(nullptr));
    return ack;
  }
  return nullptr;
}

/**
 * @brief Update a service in the cache.
 *
 * @param svc The adaptive service used to update the service.
 */
void broker_cache::update_service(
    const std::shared_ptr<neb::pb_adaptive_service>& svc) {
  if (!section_enabled(CACHE_SERVICES))
    return;
  absl::WriterMutexLock l{&_mutex};

  auto& as = svc->obj();
  auto& index = _services.get<by_id>();
  auto it = index.find(std::make_pair(as.host_id(), as.service_id()));
  if (it != _services.end()) {
    /* The cached service is shared with readers that no longer hold the mutex:
     * clone it, change the clone, then swap. */
    auto sp = std::make_shared<neb::pb_service>(*it->get());
    auto& s = sp->mut_obj();
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
    index.replace(it, sp);
  } else {
    SPDLOG_LOGGER_WARN(
        _logger,
        "Cannot update cache for service ({}, {}), it does not exist in "
        "the cache",
        as.host_id(), as.service_id());
  }
}

/**
 * @brief Update a service in the cache.
 *
 * @param ass The adaptive service status used to update the service.
 */
void broker_cache::update_service(
    const std::shared_ptr<neb::pb_adaptive_service_status>& ass) {
  if (!section_enabled(CACHE_SERVICES))
    return;

  std::shared_ptr<neb::pb_acknowledgement> ack_to_close;
  {
    absl::WriterMutexLock l{&_mutex};

    const auto& obj = ass->obj();

    SPDLOG_LOGGER_DEBUG(_logger, "Processing adaptive service status ({}, {})",
                        obj.host_id(), obj.service_id());
    auto& index = _services.get<by_id>();

    auto it = index.find(std::make_pair(obj.host_id(), obj.service_id()));
    if (it == _services.end()) {
      SPDLOG_LOGGER_WARN(
          _logger,
          "Attempt to update service ({}, {}) in global cache, but it does not "
          "exist.",
          obj.host_id(), obj.service_id());
      return;
    }

    /* The cached service is shared with readers that no longer hold the mutex:
     * clone it, change the clone, then swap. */
    auto s = std::make_shared<neb::pb_service>(*it->get());
    auto& svc = s->mut_obj();
    if (obj.has_acknowledgement_type())
      svc.set_acknowledgement_type(obj.acknowledgement_type());
    if (obj.has_scheduled_downtime_depth())
      svc.set_scheduled_downtime_depth(obj.scheduled_downtime_depth());
    if (obj.has_notification_number())
      svc.set_notification_number(obj.notification_number());
    index.replace(it, s);

    /* Acknowledgement event: decide under the lock, publish after release. */
    ack_to_close = _take_expired_acknowledgement(
        obj.host_id(), obj.service_id(), svc.acknowledgement_type(),
        static_cast<uint16_t>(svc.state()));
  }
  if (ack_to_close) {
    multiplexing::publisher pblshr;
    pblshr.write(std::move(ack_to_close));
  }
}

/**
 * @brief Update a severity level in the cache from a NEB event. The existing
 * db_id is preserved so that the database foreign key is not lost.
 *
 * @param evt The severity event carrying the updated level.
 */
void broker_cache::update_severity(
    const std::shared_ptr<neb::pb_severity>& evt) {
  if (!section_enabled(CACHE_SEVERITIES))
    return;
  auto& obj = evt->obj();
  absl::WriterMutexLock lck{&_mutex};
  auto it = _severities.find({obj.id(), obj.type()});
  if (it != _severities.end()) {
    it->second.first.level = obj.level();  // preserve existing db id
    it->second.second.insert(obj.poller_id());
  } else
    _severities.insert(
        {{obj.id(), obj.type()},
         {{obj.level(), 0}, absl::flat_hash_set<uint64_t>{obj.poller_id()}}});
}

/**
 * @brief Return a snapshot of all severities currently held in the cache.
 *
 * @return A copy of the internal severities map, keyed by (config_id, type).
 */
absl::flat_hash_map<std::pair<uint64_t, uint32_t>,
                    struct broker_cache::severity>
broker_cache::severities() const {
  absl::flat_hash_map<std::pair<uint64_t, uint32_t>,
                      struct broker_cache::severity>
      retval;
  absl::ReaderMutexLock lck{&_mutex};
  for (auto& [key, val] : _severities)
    retval.emplace(key, val.first);

  return retval;
}

/**
 * @brief Return a snapshot of all tags currently held in the cache.
 *
 * @return A copy of the internal tags map, keyed by (tag_id, tag_type).
 */
absl::flat_hash_map<
    std::pair<uint64_t, TagType>,
    std::pair<std::shared_ptr<neb::pb_tag>, absl::flat_hash_set<uint64_t>>>
broker_cache::tags() const {
  absl::ReaderMutexLock lck{&_mutex};
  return _tags;
}

/**
 * @brief Update a tag in the cache from a NEB event.
 *
 * @param evt The pb_tag event (ADD, MODIFY or DELETE).
 */
void broker_cache::update_tag(const std::shared_ptr<neb::pb_tag>& evt) {
  if (!section_enabled(CACHE_TAGS))
    return;
  auto& obj = evt->obj();
  auto key = std::make_pair(obj.id(), obj.type());
  absl::WriterMutexLock lck{&_mutex};
  if (obj.action() == Tag_Action_DELETE)
    _tags.erase(key);
  else {
    auto it = _tags.find(key);
    if (it != _tags.end())
      it->second.first = evt;
    else
      _tags.emplace(key, std::make_pair(evt, absl::flat_hash_set<uint64_t>{}));
  }
}

/**
 * @brief Get a tag by its id and type.
 *
 * @param tag_id The tag id.
 * @param type   The tag type.
 * @return A shared pointer to the pb_tag, or nullptr if not found.
 */
std::shared_ptr<neb::pb_tag> broker_cache::get_tag(uint64_t tag_id,
                                                   TagType type) const {
  absl::ReaderMutexLock lck{&_mutex};
  auto it = _tags.find({tag_id, type});
  return it != _tags.end() ? it->second.first : nullptr;
}

/**
 * @brief Get the sorted list of tag IDs of a given type for a host.
 *
 * @param host_id The host ID.
 * @param type    The tag type to filter on.
 * @return Sorted vector of tag IDs.
 */
std::vector<uint64_t> broker_cache::host_tag_ids(uint64_t host_id,
                                                 TagType type) const {
  absl::ReaderMutexLock lck{&_mutex};
  std::vector<uint64_t> result;
  auto it = _hosts.get<by_id>().find(host_id);
  if (it == _hosts.get<by_id>().end())
    return result;
  for (const auto& t : (*it)->obj().tags()) {
    if (t.type() == type)
      result.push_back(t.id());
  }
  std::sort(result.begin(), result.end());
  return result;
}

/**
 * @brief Get the sorted list of tag names of a given type for a host.
 * Names are sorted by ascending tag ID to match global_cache ordering.
 *
 * @param host_id The host ID.
 * @param type    The tag type to filter on.
 * @return Vector of tag names sorted by tag ID.
 */
std::vector<std::string> broker_cache::host_tag_names(uint64_t host_id,
                                                      TagType type) const {
  absl::ReaderMutexLock lck{&_mutex};
  std::vector<std::pair<uint64_t, std::string>> pairs;
  auto it = _hosts.get<by_id>().find(host_id);
  if (it == _hosts.get<by_id>().end())
    return {};
  for (const auto& t : (*it)->obj().tags()) {
    if (t.type() != type)
      continue;
    auto tag_it = _tags.find({t.id(), type});
    if (tag_it != _tags.end())
      pairs.emplace_back(t.id(), tag_it->second.first->obj().name());
  }
  std::sort(pairs.begin(), pairs.end());
  std::vector<std::string> result;
  result.reserve(pairs.size());
  for (auto& p : pairs)
    result.push_back(std::move(p.second));
  return result;
}

/**
 * @brief Get the sorted list of tag IDs of a given type for a service.
 *
 * @param host_id    The host ID.
 * @param service_id The service ID.
 * @param type       The tag type to filter on.
 * @return Sorted vector of tag IDs.
 */
std::vector<uint64_t> broker_cache::service_tag_ids(uint64_t host_id,
                                                    uint64_t service_id,
                                                    TagType type) const {
  absl::ReaderMutexLock lck{&_mutex};
  std::vector<uint64_t> result;
  auto& index = _services.get<by_id>();
  auto it = index.find(std::make_pair(host_id, service_id));
  if (it == index.end())
    return result;
  for (const auto& t : (*it)->obj().tags()) {
    if (t.type() == type)
      result.push_back(t.id());
  }
  std::sort(result.begin(), result.end());
  return result;
}

/**
 * @brief Get the sorted list of tag names of a given type for a service.
 * Names are sorted by ascending tag ID to match global_cache ordering.
 *
 * @param host_id    The host ID.
 * @param service_id The service ID.
 * @param type       The tag type to filter on.
 * @return Vector of tag names sorted by tag ID.
 */
std::vector<std::string> broker_cache::service_tag_names(uint64_t host_id,
                                                         uint64_t service_id,
                                                         TagType type) const {
  absl::ReaderMutexLock lck{&_mutex};
  std::vector<std::pair<uint64_t, std::string>> pairs;
  auto& index = _services.get<by_id>();
  auto it = index.find(std::make_pair(host_id, service_id));
  if (it == index.end())
    return {};
  for (const auto& t : (*it)->obj().tags()) {
    if (t.type() != type)
      continue;
    auto tag_it = _tags.find({t.id(), type});
    if (tag_it != _tags.end())
      pairs.emplace_back(t.id(), tag_it->second.first->obj().name());
  }
  std::sort(pairs.begin(), pairs.end());
  std::vector<std::string> result;
  result.reserve(pairs.size());
  for (auto& p : pairs)
    result.push_back(std::move(p.second));
  return result;
}

/**
 * @brief Record the database auto-increment ID assigned to a severity.
 *
 * Called once a severity has been inserted in the database, so later events
 * referencing it can be linked to the right row. If no cache entry exists yet
 * (e.g. the db_id is learned at startup before the config events arrive), a
 * stub entry is created so the db_id is not lost.
 *
 * @param config_id The severity configuration ID.
 * @param type The severity type (0=service, 1=host).
 * @param db_id The database auto-increment ID to store.
 */
void broker_cache::set_db_id_for_severity(uint64_t config_id,
                                          uint32_t type,
                                          uint64_t db_id) {
  if (!section_enabled(CACHE_SEVERITIES))
    return;
  absl::WriterMutexLock lck{&_mutex};
  auto it = _severities.find({config_id, type});
  if (it != _severities.end())
    it->second.first.db_id = db_id;
  else
    // Entry may not exist yet (e.g. loaded from DB at startup before config
    // events arrive): create a stub entry so the db_id is not lost.
    _severities.insert(
        {{config_id, type}, {{0, db_id}, absl::flat_hash_set<uint64_t>{}}});
}

/**
 * @brief Remove a severity from the cache by its config ID and type.
 *
 * Called when a severity is deleted from the database so the cache no longer
 * holds a stale entry for it.
 *
 * @param config_id The severity configuration ID.
 * @param type The severity type (0=service, 1=host).
 */
void broker_cache::erase_severity(uint64_t config_id, uint32_t type) {
  if (!section_enabled(CACHE_SEVERITIES))
    return;
  absl::WriterMutexLock lck{&_mutex};
  _severities.erase({config_id, type});
}

/**
 * @brief Tell whether a severity is present in the cache.
 *
 * @param config_id The severity configuration ID.
 * @param type The severity type (0=service, 1=host).
 * @return True if the severity is cached (always false when the severities
 * section is disabled).
 */
bool broker_cache::has_severity(uint64_t config_id, uint32_t type) const {
  if (!section_enabled(CACHE_SEVERITIES))
    return false;
  absl::ReaderMutexLock lck{&_mutex};
  return _severities.contains({config_id, type});
}

/**
 * @brief Get the database auto-increment ID for a severity identified by its
 * config ID and type.
 *
 * @param severity_id The severity configuration ID.
 * @param type The severity type (0=service, 1=host).
 * @return The database ID, or 0 if the severity is not found in the cache.
 */
uint64_t broker_cache::get_db_id_for_severity(uint64_t severity_id,
                                              uint32_t type) {
  absl::ReaderMutexLock lck{&_mutex};
  auto it = _severities.find({severity_id, type});
  if (it != _severities.end())
    return it->second.first.db_id;
  else {
    SPDLOG_LOGGER_WARN(_logger,
                       "Attempt to get severity ID for severity with key ({}, "
                       "{}), but it does not exist in cache.",
                       severity_id, type);
    return 0;
  }
}

/**
 * @brief Update a BA/BV relation in the cache.
 *
 * @param rel The dimension BA/BV relation event to store.
 */
void broker_cache::update_dimension_ba_bv_relation(
    const std::shared_ptr<bam::pb_dimension_ba_bv_relation_event>& rel) {
  if (!section_enabled(CACHE_BAM))
    return;
  absl::WriterMutexLock l(&_mutex);
  const auto& obj = rel->obj();
  _dimension_ba_bv_relations.emplace(std::make_pair(obj.ba_id(), obj.bv_id()));
}

/**
 * @brief Get the list of BV IDs associated with a given BA ID.
 *
 * @param ba_id The business activity ID.
 * @return A vector of BV IDs linked to this BA.
 */
std::vector<uint64_t> broker_cache::dimension_bvs_for_ba(uint64_t ba_id) const {
  absl::ReaderMutexLock l(&_mutex);
  std::vector<uint64_t> retval;
  auto lower = _dimension_ba_bv_relations.lower_bound({ba_id, 0});
  auto upper = _dimension_ba_bv_relations.upper_bound({ba_id + 1, 0});
  retval.reserve(std::distance(lower, upper));
  for (auto it = lower; it != upper; ++it)
    retval.push_back(it->second);

  return retval;
}

/**
 * @brief Update a dimension business view event in the cache.
 *
 * @param bv_event The dimension business view event to update
 */
void broker_cache::update_dimension_bv_event(
    const std::shared_ptr<bam::pb_dimension_bv_event>& bv_event) {
  if (!section_enabled(CACHE_BAM))
    return;
  absl::WriterMutexLock l(&_mutex);
  const auto& obj = bv_event->obj();
  _dimension_bvs.insert_or_assign(obj.bv_id(), bv_event);
}

/**
 * @brief Get the dimension business view event of the given ID from the
 * cache.
 *
 * @param bv_id The business view ID of the desired event.
 *
 * @return A shared pointer to the event, nullptr if not found.
 */
std::shared_ptr<bam::pb_dimension_bv_event> broker_cache::dimension_bv(
    uint64_t bv_id) const {
  absl::ReaderMutexLock l(&_mutex);
  auto found = _dimension_bvs.find(bv_id);
  if (found == _dimension_bvs.end())
    return nullptr;
  else
    return found->second;
}

/**
 * @brief Update a dimension business activity event in the cache.
 *
 * @param ba_event The dimension business activity event to update.
 */
void broker_cache::update_dimension_ba_event(
    const std::shared_ptr<bam::pb_dimension_ba_event>& ba_event) {
  if (!section_enabled(CACHE_BAM))
    return;
  absl::WriterMutexLock l(&_mutex);
  const auto& obj = ba_event->obj();
  _dimension_bas.insert_or_assign(obj.ba_id(), ba_event);
}

/**
 * @brief Get the dimension business activity event for the given BA ID.
 *
 * @param ba_id The business activity ID.
 * @return A shared pointer to the event, nullptr if not found.
 */
std::shared_ptr<bam::pb_dimension_ba_event> broker_cache::dimension_ba(
    uint64_t ba_id) const {
  absl::ReaderMutexLock l(&_mutex);
  auto found = _dimension_bas.find(ba_id);
  if (found == _dimension_bas.end())
    return nullptr;
  else
    return found->second;
}

/**
 * @brief Get the poller instance of the given ID from the cache.
 *
 * @param instance_id The poller ID of the desired instance.
 *
 * @return A shared pointer to the instance, nullptr if not found.
 */
std::string broker_cache::instance(uint64_t instance_id) const {
  absl::ReaderMutexLock l{&_mutex};
  auto found = _instances.find(instance_id);
  if (found == _instances.end())
    return "";
  else
    return found->second.name;
}

/**
 * @brief Tell whether notifications are enabled for the given poller.
 *
 * @param instance_id The poller ID.
 *
 * @return true if notifications are enabled (also the default when the poller
 * is unknown).
 */
bool broker_cache::notifications_enabled(uint64_t instance_id) const {
  absl::ReaderMutexLock l{&_mutex};
  auto found = _instances.find(instance_id);
  if (found == _instances.end())
    return true;
  return found->second.notifications_enabled;
}

/**
 * @brief Tell whether the given poller sends recovery notifications even when
 * no problem notification was sent.
 *
 * @param instance_id The poller ID.
 *
 * @return true if the poller is configured to send recovery notifications
 * anyway (false when the poller is unknown, matching the Engine default).
 */
bool broker_cache::send_recovery_notifications_anyway(
    uint64_t instance_id) const {
  absl::ReaderMutexLock l{&_mutex};
  auto found = _instances.find(instance_id);
  if (found == _instances.end())
    return false;
  return found->second.send_recovery_notifications_anyway;
}

/**
 * @brief Whether the poller reads a soft master state as its last hard state
 * when evaluating notification dependencies.
 *
 * @param instance_id The poller ID.
 *
 * @return The poller's soft_state_dependencies flag (false when the poller is
 * unknown, matching the Engine default).
 */
bool broker_cache::soft_state_dependencies(uint64_t instance_id) const {
  absl::ReaderMutexLock l{&_mutex};
  auto found = _instances.find(instance_id);
  if (found == _instances.end())
    return false;
  return found->second.soft_state_dependencies;
}

/**
 * @brief List the notification dependencies of a dependent host.
 *
 * @param dependent_host_id The id of the depending host.
 *
 * @return The cached notification dependencies whose dependent is this host.
 */
std::vector<host_notif_dep> broker_cache::host_notif_dependencies(
    uint64_t dependent_host_id) const {
  absl::ReaderMutexLock l{&_mutex};
  std::vector<host_notif_dep> result;
  const auto& idx = _host_notif_deps.get<by_dependent>();
  auto range = idx.equal_range(dependent_host_id);
  for (auto it = range.first; it != range.second; ++it)
    result.push_back(*it);
  return result;
}

/**
 * @brief List the notification dependencies of a dependent service.
 *
 * @param dependent_host_id The host id of the depending service.
 * @param dependent_service_id The service id of the depending service.
 *
 * @return The cached notification dependencies whose dependent is this service.
 */
std::vector<service_notif_dep> broker_cache::service_notif_dependencies(
    uint64_t dependent_host_id,
    uint64_t dependent_service_id) const {
  absl::ReaderMutexLock l{&_mutex};
  std::vector<service_notif_dep> result;
  const auto& idx = _service_notif_deps.get<by_dependent>();
  auto range =
      idx.equal_range(std::make_pair(dependent_host_id, dependent_service_id));
  for (auto it = range.first; it != range.second; ++it)
    result.push_back(*it);
  return result;
}

/**
 * @brief Look up a cached notification contact by name.
 *
 * @param name The contact name.
 *
 * @return The cached contact, or std::nullopt when no contact of that name is
 * known to the cache.
 */
std::optional<broker_cache::contact> broker_cache::contact_config(
    const std::string& name) const {
  absl::ReaderMutexLock l{&_mutex};
  auto it = _contacts.find(name);
  if (it == _contacts.end())
    return std::nullopt;
  return it->second.first;
}

/**
 * @brief Expand a contactgroup to the names of its member contacts.
 *
 * This is the contactgroup->contacts resolution done at notification time: the
 * group members are read straight from the cache instead of being flattened
 * onto the resources or contacts at configuration time.
 *
 * @param name The contactgroup name.
 *
 * @return The member contact names; empty when the group is unknown or has no
 * member.
 */
std::vector<std::string> broker_cache::contactgroup_members(
    const std::string& name) const {
  absl::ReaderMutexLock l{&_mutex};
  std::vector<std::string> result;
  auto it = _contactgroups.find(name);
  if (it == _contactgroups.end())
    return result;
  const auto& members = it->second.first.members;
  result.reserve(members.size());
  for (const std::string& member : members)
    result.push_back(member);
  return result;
}

/**
 * @brief Evaluate the notification dependencies of a resource.
 *
 * Broker counterpart of Engine's host/service::authorized_by_dependencies for
 * dependency::notification. Takes the read lock once and dispatches to the
 * recursive per-type helpers (which require the lock held: absl reader locks
 * are not reentrant, so the inherits_parent recursion must not re-lock).
 *
 * @param host_id The host id.
 * @param service_id The service id; 0 designates a host.
 *
 * @return false when a master resource fails the dependency test; true
 * otherwise (including for a resource unknown to the cache).
 */
bool broker_cache::notification_authorized_by_dependencies(
    uint64_t host_id,
    uint64_t service_id) const {
  absl::ReaderMutexLock l{&_mutex};
  std::time_t now = std::time(nullptr);
  return service_id
             ? _service_notification_authorized_by_dependencies(host_id,
                                                                service_id, now)
             : _host_notification_authorized_by_dependencies(host_id, now);
}

/**
 * @brief Notification-dependency evaluation for a host (see
 * notification_authorized_by_dependencies). The lock must be held by the
 * caller.
 *
 * @param host_id The dependent host id.
 * @param now The reference time for the dependency-period checks.
 *
 * @return Whether the host's notification dependencies authorize a
 * notification.
 */
bool broker_cache::_host_notification_authorized_by_dependencies(
    uint64_t host_id,
    std::time_t now) const {
  const auto& hosts_by_id = _hosts.get<by_id>();
  auto dependent = hosts_by_id.find(host_id);
  if (dependent == hosts_by_id.end())
    return true;

  const absl::TimeZone tz =
      common::timeperiods::string_to_timezone((*dependent)->obj().timezone());

  /* action_hd_* bits are aligned with the neb host state enum
   * (UP=0, DOWN=1, UNREACHABLE=2). */
  static constexpr std::array<uint32_t, 3> fail_bit{
      engine::configuration::action_hd_up,
      engine::configuration::action_hd_down,
      engine::configuration::action_hd_unreachable};

  const auto& deps = _host_notif_deps.get<by_dependent>();
  for (auto [it, end] = deps.equal_range(host_id); it != end; ++it) {
    const host_notif_dep& dep = *it;
    auto master = hosts_by_id.find(dep.master_host_id);
    if (master == hosts_by_id.end())
      continue;
    const Host& m = (*master)->obj();

    /* The dependency does not apply when now is outside its dependency
     * period. */
    if (!dep.dependency_period.empty()) {
      auto tp = _timeperiods.find(dep.dependency_period);
      if (tp != _timeperiods.end() &&
          !tp->second->check_time_against_period(now, tz))
        return true;
    }

    bool soft_state_dependencies = false;
    if (auto inst = _instances.find(dep.poller_id); inst != _instances.end())
      soft_state_dependencies = inst->second.soft_state_dependencies;

    /* Use the last hard state when the master is in a soft state, unless the
     * poller opted into soft-state dependencies. */
    int state = (m.state_type() == Host::SOFT && !soft_state_dependencies)
                    ? m.last_hard_state()
                    : m.state();

    if (state >= 0 && state < static_cast<int>(fail_bit.size()) &&
        (dep.notification_failure_options & fail_bit[state]))
      return false;

    if (state == Host::UP && !m.checked() &&
        (dep.notification_failure_options &
         engine::configuration::action_hd_pending))
      return false;

    if (dep.inherits_parent &&
        !_host_notification_authorized_by_dependencies(dep.master_host_id, now))
      return false;
  }
  return true;
}

/**
 * @brief Notification-dependency evaluation for a service (see
 * notification_authorized_by_dependencies). The lock must be held by the
 * caller.
 *
 * @param host_id The dependent service's host id.
 * @param service_id The dependent service id.
 * @param now The reference time for the dependency-period checks.
 *
 * @return Whether the service's notification dependencies authorize a
 * notification.
 */
bool broker_cache::_service_notification_authorized_by_dependencies(
    uint64_t host_id,
    uint64_t service_id,
    std::time_t now) const {
  const auto& svc_by_id = _services.get<by_id>();
  auto dependent = svc_by_id.find(std::make_pair(host_id, service_id));
  if (dependent == svc_by_id.end())
    return true;

  /* Notification timezone inheritance: an empty service timezone falls back to
   * the host's, mirroring get_state. */
  std::string tz_str = (*dependent)->obj().timezone();
  if (tz_str.empty()) {
    const auto& hosts_by_id = _hosts.get<by_id>();
    auto h = hosts_by_id.find(host_id);
    if (h != hosts_by_id.end())
      tz_str = (*h)->obj().timezone();
  }
  const absl::TimeZone tz = common::timeperiods::string_to_timezone(tz_str);

  /* action_sd_* bit order differs from the neb service state enum
   * (OK=0, WARNING=1, CRITICAL=2, UNKNOWN=3), so map explicitly. */
  static constexpr std::array<uint32_t, 4> fail_bit{
      engine::configuration::action_sd_ok,
      engine::configuration::action_sd_warning,
      engine::configuration::action_sd_critical,
      engine::configuration::action_sd_unknown};

  const auto& deps = _service_notif_deps.get<by_dependent>();
  for (auto [it, end] = deps.equal_range(std::make_pair(host_id, service_id));
       it != end; ++it) {
    const service_notif_dep& dep = *it;
    auto master = svc_by_id.find(
        std::make_pair(dep.master_host_id, dep.master_service_id));
    if (master == svc_by_id.end())
      continue;
    const Service& m = (*master)->obj();

    if (!dep.dependency_period.empty()) {
      auto tp = _timeperiods.find(dep.dependency_period);
      if (tp != _timeperiods.end() &&
          !tp->second->check_time_against_period(now, tz))
        return true;
    }

    bool soft_state_dependencies = false;
    if (auto inst = _instances.find(dep.poller_id); inst != _instances.end())
      soft_state_dependencies = inst->second.soft_state_dependencies;

    int state = (m.state_type() == Service::SOFT && !soft_state_dependencies)
                    ? m.last_hard_state()
                    : m.state();

    if (state >= 0 && state < static_cast<int>(fail_bit.size()) &&
        (dep.notification_failure_options & fail_bit[state]))
      return false;

    if (state == Service::OK && !m.checked() &&
        (dep.notification_failure_options &
         engine::configuration::action_sd_pending))
      return false;

    if (dep.inherits_parent &&
        !_service_notification_authorized_by_dependencies(
            dep.master_host_id, dep.master_service_id, now))
      return false;
  }
  return true;
}

/**
 * @brief Get the duration of one "interval unit" for the given poller.
 *
 * @param instance_id The poller ID.
 *
 * @return The poller's interval_length (the Engine default of 60s when the
 * poller is unknown).
 */
std::chrono::seconds broker_cache::interval_length(uint64_t instance_id) const {
  absl::ReaderMutexLock l{&_mutex};
  auto found = _instances.find(instance_id);
  if (found == _instances.end())
    return instance_info::default_interval_length;
  return found->second.interval_length;
}

/**
 * @brief (Re)link the exclusion pointers of every cached timeperiod.
 *
 * Called after any change to _timeperiods (merge()/apply()). A timeperiod's
 * exclusions hold raw pointers into sibling timeperiods; when insert_or_assign
 * replaces an object, every timeperiod excluding it must be re-linked, so the
 * whole set is resolved against itself. Resolution errors (an exclusion naming
 * an unknown timeperiod) are logged and skipped per object rather than
 * propagated, so a single bad configuration cannot abort the whole merge.
 *
 * Must be called with _mutex held for writing.
 */
void broker_cache::_resolve_timeperiods() {
  uint32_t warnings = 0, errors = 0;
  for (auto& [name, tp] : _timeperiods) {
    try {
      tp->resolve(_timeperiods, warnings, errors);
    } catch (const std::exception& e) {
      _logger->error("broker_cache: cannot resolve timeperiod '{}': {}", name,
                     e.what());
    }
  }
}

/**
 * @brief Tell whether @p now falls within the given notification timeperiod.
 *
 * The cache-owned _timeperiods map is only mutated by merge()/apply() under
 * _mutex, so evaluating under the reader lock keeps the whole check race-free.
 *
 * @param period_name The notification timeperiod name; an empty or unknown
 * name means notifications are allowed at any time.
 * @param timezone The resource timezone directive (possibly empty).
 * @param now The time to test.
 *
 * @return true if notifications are allowed at @p now.
 */
bool broker_cache::in_notification_period(const std::string& period_name,
                                          const std::string& timezone,
                                          std::time_t now) const {
  absl::ReaderMutexLock l{&_mutex};
  auto it = _timeperiods.find(period_name);
  return it == _timeperiods.end() ||
         it->second->check_time_against_period_for_notif(
             now, common::timeperiods::string_to_timezone(timezone));
}

/**
 * @brief Get the host of the given name from the cache.
 *
 * @param host_name The host name of the desired host.
 *
 * @return A shared pointer to the host, nullptr if not found.
 */
std::shared_ptr<neb::pb_host> broker_cache::host(
    const std::string& host_name) const {
  absl::ReaderMutexLock l{&_mutex};
  auto& index = _hosts.get<by_name>();
  auto found = index.find(host_name);
  if (found == index.end())
    return nullptr;
  else
    return *found;
}

/**
 * @brief Get the host of the given ID from the cache.
 *
 * @param host_id The host ID of the desired host.
 *
 * @return A shared pointer to the host, nullptr if not found.
 */
std::shared_ptr<neb::pb_host> broker_cache::host(uint64_t host_id) const {
  absl::ReaderMutexLock l{&_mutex};
  auto& index = _hosts.get<by_id>();
  auto found = index.find(host_id);
  if (found == index.end())
    return nullptr;
  else
    return *found;
}

/**
 * @brief Get the list of host IDs present in the cache.
 *
 * @return A vector of host IDs.
 */
std::vector<uint64_t> broker_cache::host_ids() const {
  absl::ReaderMutexLock l{&_mutex};
  std::vector<uint64_t> retval;
  auto& index = _hosts.get<by_id>();
  retval.reserve(index.size());
  for (const auto& host : _hosts)
    retval.push_back(host->obj().host_id());
  return retval;
}

/**
 * @brief Get the service of the given host ID and service ID from the cache.
 *
 * @param host_id The host ID of the desired service.
 * @param service_id The service ID of the desired service.
 *
 * @return A shared pointer to the service, nullptr if not found.
 */
std::shared_ptr<neb::pb_service> broker_cache::service(
    const std::string& hostname,
    const std::string& description) const {
  absl::ReaderMutexLock l{&_mutex};
  auto& index = _services.get<by_name>();
  auto found = index.find(std::make_pair(hostname, description));
  if (found == index.end())
    return nullptr;
  else
    return *found;
}

/**
 * @brief Get the service of the given host ID and service ID from the cache.
 *
 * @param host_id The host ID of the desired service.
 * @param service_id The service ID of the desired service.
 *
 * @return A shared pointer to the service, nullptr if not found.
 */
std::shared_ptr<neb::pb_service> broker_cache::service(
    uint64_t host_id,
    uint64_t service_id) const {
  absl::ReaderMutexLock l{&_mutex};
  auto& index = _services.get<by_id>();
  auto found = index.find(std::make_pair(host_id, service_id));
  if (found == index.end())
    return nullptr;
  else
    return *found;
}

/**
 * @brief Get an index mapping by its index ID.
 *
 * @param index_id The index ID to look up.
 * @return A shared pointer to the index mapping, nullptr if not found.
 */
std::shared_ptr<storage::pb_index_mapping> broker_cache::get_index_mapping(
    uint64_t index_id) const {
  absl::ReaderMutexLock l{&_mutex};
  auto& index = _index_mappings.get<by_id>();
  auto found = index.find(index_id);
  if (found == index.end())
    return nullptr;
  else
    return *found;
}

/**
 * @brief Get an index mapping by host ID and service ID.
 *
 * @param host_id The host ID.
 * @param service_id The service ID.
 * @return A shared pointer to the index mapping, nullptr if not found.
 */
std::shared_ptr<storage::pb_index_mapping> broker_cache::get_index_mapping(
    uint64_t host_id,
    uint64_t service_id) const {
  absl::ReaderMutexLock l{&_mutex};
  auto& index = _index_mappings.get<by_service>();
  auto found = index.find(std::make_pair(host_id, service_id));
  if (found == index.end())
    return nullptr;
  else
    return *found;
}

/**
 * @brief Retrieve the metric mapping for a given metric ID.
 *
 * @param metric_id The metric ID to look up.
 * @return A shared pointer to the pb_metric_mapping, or nullptr if not found.
 */
std::shared_ptr<storage::pb_metric_mapping> broker_cache::get_metric_mapping(
    uint64_t metric_id) const {
  absl::ReaderMutexLock l{&_mutex};
  auto found = _metric_mappings.find(metric_id);
  if (found == _metric_mappings.end())
    return nullptr;
  else
    return found->second;
}

/**
 * @brief Get the list of service IDs present in the cache.
 *
 * @return A vector of pairs of host ID and service ID.
 */
std::vector<std::pair<uint64_t, uint64_t>> broker_cache::service_ids() const {
  absl::ReaderMutexLock l{&_mutex};
  std::vector<std::pair<uint64_t, uint64_t>> retval;
  retval.reserve(_services.size());
  for (const auto& id : _services) {
    auto p = std::make_pair(id->obj().host_id(), id->obj().service_id());
    retval.push_back(p);
  }
  return retval;
}

/**
 * @brief Return the ID of the first non-zero active Engine instance known to
 * the cache.
 *
 * Used as a fallback when a cached host object has instance_id == 0 (which
 * happens when the State message sent by Engine does not carry poller_id on
 * individual host objects). In single-poller setups this reliably returns the
 * only running poller ID.
 *
 * @return The first non-zero instance ID, or 0 if no instance is known yet.
 */
uint32_t broker_cache::first_active_instance_id() const {
  absl::ReaderMutexLock l{&_mutex};
  for (const auto& [id, info] : _instances) {
    if (id != 0)
      return id;
  }
  return 0;
}

/**
 * @brief Return the service IDs of all services belonging to a given host.
 *
 * Uses the ordered by_id index on _services, which is sorted by
 * {host_id, service_id}, so services of the same host are contiguous and can
 * be retrieved with a single lower_bound scan.
 *
 * @param host_id The host whose services are requested.
 * @return A vector of service IDs. Empty if the host has no services or is
 *         unknown.
 */
std::vector<uint64_t> broker_cache::service_ids_for_host(
    uint64_t host_id) const {
  absl::ReaderMutexLock l{&_mutex};
  std::vector<uint64_t> result;
  auto& index = _services.get<by_id>();
  for (auto it = index.lower_bound(std::make_pair(host_id, uint64_t(0)));
       it != index.end() && (*it)->obj().host_id() == host_id; ++it)
    result.push_back((*it)->obj().service_id());
  return result;
}

/**
 * @brief Increment the scheduled_downtime_depth counter of a host or service
 * in the cache and return the new depth.
 *
 * Called by broker_downtime_callbacks::start_downtime_effect() when a
 * downtime becomes active. The updated depth is then published as a
 * pb_adaptive_host_status or pb_adaptive_service_status event so that
 * unified_sql can persist it.
 *
 * @param host_id    The host ID.
 * @param service_id The service ID, or 0 for a host downtime.
 * @return The new depth after increment, or 0 if the object is not in cache.
 */
int32_t broker_cache::add_downtime(uint64_t host_id, uint64_t service_id) {
  absl::WriterMutexLock l{&_mutex};
  if (service_id == 0) {
    auto& index = _hosts.get<by_id>();
    auto it = index.find(host_id);
    if (it == index.end())
      return 0;
    int32_t depth = (*it)->obj().scheduled_downtime_depth() + 1;
    auto h = std::make_shared<neb::pb_host>(**it);
    h->mut_obj().set_scheduled_downtime_depth(depth);
    index.replace(it, h);
    return depth;
  }
  auto& index = _services.get<by_id>();
  auto it = index.find(std::make_pair(host_id, service_id));
  if (it == index.end())
    return 0;
  int32_t depth = (*it)->obj().scheduled_downtime_depth() + 1;
  auto s = std::make_shared<neb::pb_service>(**it);
  s->mut_obj().set_scheduled_downtime_depth(depth);
  index.replace(it, s);
  return depth;
}

/**
 * @brief Decrement the scheduled_downtime_depth counter of a host or service
 * in the cache and return the new depth (clamped to 0).
 *
 * Called by broker_downtime_callbacks::end_downtime_effect() and
 * cancel_downtime() when a downtime ends or is cancelled. The updated depth
 * is then published as a pb_adaptive_host_status or
 * pb_adaptive_service_status event.
 *
 * @param host_id    The host ID.
 * @param service_id The service ID, or 0 for a host downtime.
 * @return The new depth after decrement (>= 0), or 0 if the object is not in
 *         cache.
 */
int32_t broker_cache::remove_downtime(uint64_t host_id, uint64_t service_id) {
  absl::WriterMutexLock l{&_mutex};
  if (service_id == 0) {
    auto& index = _hosts.get<by_id>();
    auto it = index.find(host_id);
    if (it == index.end())
      return 0;
    int32_t depth = std::max(0, (*it)->obj().scheduled_downtime_depth() - 1);
    auto h = std::make_shared<neb::pb_host>(**it);
    h->mut_obj().set_scheduled_downtime_depth(depth);
    index.replace(it, h);
    return depth;
  }
  auto& index = _services.get<by_id>();
  auto it = index.find(std::make_pair(host_id, service_id));
  if (it == index.end())
    return 0;
  int32_t depth = std::max(0, (*it)->obj().scheduled_downtime_depth() - 1);
  auto s = std::make_shared<neb::pb_service>(**it);
  s->mut_obj().set_scheduled_downtime_depth(depth);
  index.replace(it, s);
  return depth;
}

/**
 * @brief Return the service IDs of anomaly detection services that monitor
 * a given dependent service.
 *
 * Uses the _anomaly_detection_index which maps
 * {host_id, dependent_service_id} → set<anomaly_detection_service_id>.
 * The index is maintained in sync with _services by merge() / apply() /
 * remove_instance().
 *
 * @param host_id              The host ID of the dependent service.
 * @param dependent_service_id The service ID of the service being monitored.
 * @return A vector of anomaly detection service IDs. Empty if none exist for
 *         this dependent service.
 */
std::vector<uint64_t>
broker_cache::find_anomaly_detection_ids_by_dependent_service(
    uint64_t host_id,
    uint64_t dependent_service_id) const {
  absl::ReaderMutexLock l{&_mutex};
  auto it = _anomaly_detection_index.find(
      std::make_pair(host_id, dependent_service_id));
  if (it == _anomaly_detection_index.end())
    return {};
  return {it->second.begin(), it->second.end()};
}

/**
 * @brief Get the hostgroup of the given ID from the cache.
 *
 * @param hostgroup_id The hostgroup ID of the desired hostgroup.
 *
 * @return A shared pointer to the hostgroup, nullptr if not found.
 */
std::shared_ptr<neb::pb_host_group> broker_cache::hostgroup(
    uint64_t hostgroup_id) const {
  absl::ReaderMutexLock l{&_mutex};
  auto& index = _hostgroups.get<by_id>();
  auto found = index.find(hostgroup_id);
  if (found == index.end())
    return nullptr;
  else
    return found->first;
}

/**
 * @brief Get the hostgroup of the given name from the cache.
 *
 * @param name The name of the desired hostgroup.
 *
 * @return A shared pointer to the hostgroup, nullptr if not found.
 */
std::shared_ptr<neb::pb_host_group> broker_cache::hostgroup(
    const std::string& name) const {
  absl::ReaderMutexLock l{&_mutex};
  auto& index = _hostgroups.get<by_name>();
  auto found = index.find(name);
  if (found == index.end())
    return nullptr;
  else
    return found->first;
}

/**
 * @brief Get the list of all hostgroup IDs present in the cache.
 *
 * @return A vector of hostgroup IDs.
 */
std::vector<uint64_t> broker_cache::hostgroup_ids() const {
  absl::ReaderMutexLock l{&_mutex};
  std::vector<uint64_t> retval;
  retval.reserve(_hostgroups.size());
  for (const auto& [hg, _members] : _hostgroups)
    retval.push_back(hg->obj().hostgroup_id());
  return retval;
}

/**
 * @brief Get the list of all servicegroup IDs present in the cache.
 *
 * @return A vector of servicegroup IDs.
 */
std::vector<uint64_t> broker_cache::servicegroup_ids() const {
  absl::ReaderMutexLock l{&_mutex};
  std::vector<uint64_t> retval;
  retval.reserve(_servicegroups.size());
  for (const auto& [sg, _members] : _servicegroups)
    retval.push_back(sg->obj().servicegroup_id());
  return retval;
}

/**
 * @brief Get the servicegroup of the given ID from the cache.
 *
 * @param servicegroup_id The servicegroup ID of the desired servicegroup.
 *
 * @return A shared pointer to the servicegroup, nullptr if not found.
 */
std::shared_ptr<neb::pb_service_group> broker_cache::servicegroup(
    uint64_t servicegroup_id) const {
  absl::ReaderMutexLock l{&_mutex};
  auto found = _servicegroups.find(servicegroup_id);
  if (found == _servicegroups.end())
    return nullptr;
  else
    return found->first;
}

/**
 * @brief Get the hostgroups a host belongs to from the cache.
 *
 * @param host_id The host ID of the desired host
 *
 * @return A vector of shared pointers to the hostgroups the host belongs to.
 */
std::vector<std::shared_ptr<neb::pb_host_group>> broker_cache::hostgroups(
    uint64_t host_id) const {
  absl::ReaderMutexLock l{&_mutex};
  std::vector<std::shared_ptr<neb::pb_host_group>> retval;
  auto& indexed_by_host = _host_hostgroups.get<by_host>();
  auto range = indexed_by_host.equal_range(host_id);
  retval.reserve(std::distance(range.first, range.second));
  for (auto it = range.first; it != range.second; ++it)
    retval.push_back(it->hostgroup);
  return retval;
}

/**
 * @brief Get the ids of the hosts belonging to a hostgroup.
 *
 * @param hostgroup_id The hostgroup ID.
 *
 * @return The host ids currently linked to this hostgroup in the cache.
 */
std::vector<uint64_t> broker_cache::hostgroup_members(
    uint64_t hostgroup_id) const {
  absl::ReaderMutexLock l{&_mutex};
  std::vector<uint64_t> retval;
  auto& indexed_by_hostgroup = _host_hostgroups.get<by_hostgroup>();
  auto range = indexed_by_hostgroup.equal_range(hostgroup_id);
  retval.reserve(std::distance(range.first, range.second));
  for (auto it = range.first; it != range.second; ++it)
    retval.push_back(it->host_id);
  return retval;
}

/**
 * @brief Get the {host_id, service_id} pairs of the services belonging to a
 * servicegroup.
 *
 * @param servicegroup_id The servicegroup ID.
 *
 * @return The service pairs currently linked to this servicegroup in the cache.
 */
std::vector<std::pair<uint64_t, uint64_t>> broker_cache::servicegroup_members(
    uint64_t servicegroup_id) const {
  absl::ReaderMutexLock l{&_mutex};
  std::vector<std::pair<uint64_t, uint64_t>> retval;
  auto& indexed_by_servicegroup = _service_servicegroups.get<by_servicegroup>();
  auto range = indexed_by_servicegroup.equal_range(servicegroup_id);
  retval.reserve(std::distance(range.first, range.second));
  for (auto it = range.first; it != range.second; ++it)
    retval.emplace_back(it->host_id, it->service_id);
  return retval;
}

/**
 * @brief Get the servicegroups a service belongs to from the cache.
 *
 * @param host_id The host ID of the service
 * @param service_id The service ID of the service
 *
 * @return A vector of shared pointers to the servicegroups the service
 * belongs to.
 */
std::vector<std::shared_ptr<neb::pb_service_group>> broker_cache::servicegroups(
    uint64_t host_id,
    uint64_t service_id) const {
  absl::ReaderMutexLock l{&_mutex};
  std::vector<std::shared_ptr<neb::pb_service_group>> retval;
  auto& indexed_by_service = _service_servicegroups.get<by_service>();
  auto range =
      indexed_by_service.equal_range(std::make_pair(host_id, service_id));

  retval.reserve(std::distance(range.first, range.second));
  for (auto it = range.first; it != range.second; ++it)
    retval.push_back(it->servicegroup);
  return retval;
}

/**
 * @brief Route an incoming event to the matching cache update method.
 *
 * Central dispatch of the cache: switches on the event type and calls the
 * relevant update_* /merge/apply handler, converting BBDO2 events to BBDO3
 * beforehand when needed. Events with no cache relevance fall through the
 * switch and are ignored. This is the single entry point used by both public
 * publish() overloads.
 *
 * @param evt The event to process.
 */
void broker_cache::_publish(const std::shared_ptr<io::data>& evt) {
  uint32_t type = evt->type();
  switch (type) {
    case make_type(io::neb, neb::de_host):
      update_host(
          std::static_pointer_cast<neb::pb_host>(neb::bbdo2_to_bbdo3(evt)));
      break;
    case make_type(io::neb, neb::de_service):
      update_service(
          std::static_pointer_cast<neb::pb_service>(neb::bbdo2_to_bbdo3(evt)));
      break;
    case neb::pb_host::static_type():
      update_host(std::static_pointer_cast<neb::pb_host>(evt));
      break;
    case neb::pb_host_status::static_type():
      update_host(std::static_pointer_cast<neb::pb_host_status>(evt));
      break;
    case neb::pb_adaptive_host::static_type():
      update_host(std::static_pointer_cast<neb::pb_adaptive_host>(evt));
      break;
    case neb::pb_adaptive_host_status::static_type():
      update_host(std::static_pointer_cast<neb::pb_adaptive_host_status>(evt));
      break;
    case neb::pb_service::static_type():
      update_service(std::static_pointer_cast<neb::pb_service>(evt));
      break;
    case neb::pb_service_status::static_type():
      update_service(std::static_pointer_cast<neb::pb_service_status>(evt));
      break;
    case neb::pb_adaptive_service::static_type():
      update_service(std::static_pointer_cast<neb::pb_adaptive_service>(evt));
      break;
    case neb::pb_adaptive_service_status::static_type():
      update_service(
          std::static_pointer_cast<neb::pb_adaptive_service_status>(evt));
      break;
    case neb::pb_instance::static_type():
      update_instance(std::static_pointer_cast<neb::pb_instance>(evt));
      break;
    case make_type(io::neb, neb::de_service_status):
      update_service(std::static_pointer_cast<neb::pb_service_status>(
          neb::bbdo2_to_bbdo3(evt)));
      break;
    case make_type(io::neb, neb::de_host_status):
      update_host(std::static_pointer_cast<neb::pb_host_status>(
          neb::bbdo2_to_bbdo3(evt)));
      break;
    case make_type(io::neb, neb::de_instance):
      update_instance(
          std::static_pointer_cast<neb::pb_instance>(neb::bbdo2_to_bbdo3(evt)));
      break;
    case make_type(io::neb, neb::de_host_group):
      update_hostgroup(std::static_pointer_cast<neb::pb_host_group>(
          neb::bbdo2_to_bbdo3(evt)));
      break;
    case make_type(io::neb, neb::de_pb_host_group):
      update_hostgroup(std::static_pointer_cast<neb::pb_host_group>(evt));
      break;
    case neb::pb_host_group_member::static_type():
      update_hostgroup_member(
          std::static_pointer_cast<neb::pb_host_group_member>(evt));
      break;
    case make_type(io::neb, neb::de_host_group_member):
      update_hostgroup_member(
          std::static_pointer_cast<neb::pb_host_group_member>(
              neb::bbdo2_to_bbdo3(evt)));
      break;
    case make_type(io::neb, neb::de_service_group):
      update_servicegroup(std::static_pointer_cast<neb::pb_service_group>(
          neb::bbdo2_to_bbdo3(evt)));
      break;
    case make_type(io::neb, neb::de_pb_service_group):
      update_servicegroup(std::static_pointer_cast<neb::pb_service_group>(evt));
      break;
    case make_type(io::neb, neb::de_service_group_member):
      update_servicegroup_member(
          std::static_pointer_cast<neb::pb_service_group_member>(
              neb::bbdo2_to_bbdo3(evt)));
      break;
    case make_type(io::neb, neb::de_pb_service_group_member):
      update_servicegroup_member(
          std::static_pointer_cast<neb::pb_service_group_member>(evt));
      break;
    case make_type(io::storage, storage::de_pb_metric_mapping):
      update_metric_mapping(
          std::static_pointer_cast<storage::pb_metric_mapping>(evt));
      break;
    case make_type(io::storage, storage::de_metric_mapping):
      update_metric_mapping(
          std::static_pointer_cast<storage::pb_metric_mapping>(
              neb::bbdo2_to_bbdo3(evt)));
      break;
    case make_type(io::bam, bam::de_pb_dimension_ba_bv_relation_event):
      update_dimension_ba_bv_relation(
          std::static_pointer_cast<bam::pb_dimension_ba_bv_relation_event>(
              evt));
      break;
    case make_type(io::bam, bam::de_dimension_ba_bv_relation_event):
      update_dimension_ba_bv_relation(
          std::static_pointer_cast<bam::pb_dimension_ba_bv_relation_event>(
              neb::bbdo2_to_bbdo3(evt)));
      break;
    case make_type(io::bam, bam::de_pb_dimension_ba_event):
      update_dimension_ba_event(
          std::static_pointer_cast<bam::pb_dimension_ba_event>(evt));
      break;
    case make_type(io::bam, bam::de_dimension_ba_event):
      update_dimension_ba_event(
          std::static_pointer_cast<bam::pb_dimension_ba_event>(
              neb::bbdo2_to_bbdo3(evt)));
      break;
    case make_type(io::bam, bam::de_pb_dimension_bv_event):
      update_dimension_bv_event(
          std::static_pointer_cast<bam::pb_dimension_bv_event>(evt));
      break;
    case make_type(io::bam, bam::de_dimension_bv_event):
      update_dimension_bv_event(
          std::static_pointer_cast<bam::pb_dimension_bv_event>(
              neb::bbdo2_to_bbdo3(evt)));
      break;
    case make_type(io::storage, storage::de_index_mapping):
      update_index_mapping(std::static_pointer_cast<storage::pb_index_mapping>(
          neb::bbdo2_to_bbdo3(evt)));
      break;
    case make_type(io::storage, storage::de_pb_index_mapping):
      update_index_mapping(
          std::static_pointer_cast<storage::pb_index_mapping>(evt));
      break;
    case make_type(io::neb, neb::de_pb_global_diff_state):
      apply(std::static_pointer_cast<neb::pb_global_diff_state>(evt)->obj());
      break;
    case make_type(io::neb, neb::de_pb_severity):
      update_severity(std::static_pointer_cast<neb::pb_severity>(evt));
      break;
    case make_type(io::neb, neb::de_pb_tag):
      update_tag(std::static_pointer_cast<neb::pb_tag>(evt));
      break;
    case make_type(io::neb, neb::de_acknowledgement):
      update_acknowledgement(std::static_pointer_cast<neb::pb_acknowledgement>(
          neb::bbdo2_to_bbdo3(evt)));
      break;
    case make_type(io::neb, neb::de_pb_acknowledgement):
      update_acknowledgement(
          std::static_pointer_cast<neb::pb_acknowledgement>(evt));
      break;
    default:
      break;
  }
}

/**
 * @brief Publish data to the cache. This is used by the multiplexing Engine
 * to keep retro compatibility with the legacy Broker. It is called at the
 * same time the muxers publish() are called. The goal is to populate the
 * cache.
 *
 * @param to_publish
 */
void broker_cache::publish(const std::shared_ptr<io::data>& to_publish) {
  _publish(to_publish);
}

/**
 * @brief Publish data to the cache. This is used by the multiplexing Engine
 * to keep retro compatibility with the legacy Broker. It is called at the
 * same time the muxers publish() are called. The goal is to populate the
 * cache.
 *
 * @param to_publish
 */
void broker_cache::publish(
    const std::deque<std::shared_ptr<io::data>>& to_publish) {
  for (const auto& evt : to_publish) {
    _publish(evt);
  }
}

/**
 * @brief Get the severity level of a resource (host or service) from the
 * cache.
 *
 * @param host_id The host ID of the resource
 * @param service_id The service ID of the resource (0 for host severity,
 * non-zero for service severity)
 *
 * @return The severity level of the resource, or 0 if not found or not set.
 */
uint32_t broker_cache::severity(uint64_t host_id, uint64_t service_id) const {
  absl::ReaderMutexLock l{&_mutex};
  if (service_id == 0) {
    // Host severity
    auto host_it = _hosts.get<by_id>().find(host_id);
    if (host_it != _hosts.get<by_id>().end()) {
      uint64_t severity_id = host_it->get()->obj().severity_id();
      if (severity_id) {
        auto severity_it = _severities.find({severity_id, Severity_Type_HOST});
        if (severity_it != _severities.end())
          return severity_it->second.first.level;
      } else
        throw exceptions::msg_fmt("Host {} has no severity set", host_id);
    } else {
      SPDLOG_LOGGER_WARN(_logger,
                         "Host {} not found in cache when looking for severity",
                         host_id);
    }
  } else {
    // Service severity
    auto svc_it =
        _services.get<by_id>().find(std::make_pair(host_id, service_id));
    if (svc_it != _services.get<by_id>().end()) {
      uint64_t severity_id = svc_it->get()->obj().severity_id();
      if (severity_id) {
        auto severity_it =
            _severities.find({severity_id, Severity_Type_SERVICE});
        if (severity_it != _severities.end())
          return severity_it->second.first.level;
      } else
        throw exceptions::msg_fmt("Service ({}, {}) has no severity set",
                                  host_id, service_id);
    } else {
      throw exceptions::msg_fmt(
          "Service ({}, {}) not found in cache when looking for severity",
          host_id, service_id);
    }
  }
  return 0;
}

/**
 * @brief Load the cache from a file. This is used to persist the cache across
 * restarts of the broker. The cache is loaded from a binary file using
 * Protocol Buffers serialization.
 */
void broker_cache::_load_cache() {
  SPDLOG_LOGGER_INFO(_logger, "broker_cache: loading cache from '{}'",
                     _cache_file.string());
  std::ifstream ifs{_cache_file, std::ios::binary};
  if (ifs) {
    BrokerCache to_load;
    if (!to_load.ParseFromIstream(&ifs)) {
      SPDLOG_LOGGER_ERROR(_logger, "broker_cache: cannot parse cache file '{}'",
                          _cache_file.string());
    } else {
      absl::WriterMutexLock lck{&_mutex};
      /* The heavy cache sections are reloaded only in legacy mode; in
       * centralized mode they are rebuilt from the Engine configuration. The
       * active downtimes (below) are read back in both modes. */
      if (!config::applier::state::instance().supports_centralized_conf()) {
        for (const auto& inst_pair : to_load.instances())
          _instances.insert({inst_pair.id(), instance_info{inst_pair.name()}});

        for (const auto& host : to_load.hosts()) {
          auto h = std::make_shared<neb::pb_host>();
          h->mut_obj().CopyFrom(host);
          _hosts.get<by_id>().insert(h);
        }
        for (const auto& svc : to_load.services()) {
          auto s = std::make_shared<neb::pb_service>();
          s->mut_obj().CopyFrom(svc);
          _services.get<by_id>().insert(s);
        }
        for (const auto& hgp : to_load.hostgroups()) {
          auto hst_grp = std::make_shared<neb::pb_host_group>();
          auto poller_ids = absl::flat_hash_set<uint64_t>();
          hst_grp->mut_obj().CopyFrom(hgp.hostgroup());
          /* The poller_id field is not saved in the cache, so we set it to 0
           * here. The actual poller IDs are stored in the poller_ids set, which
           * is populated from the pollers() field of the host group in the
           * cache file. */
          hst_grp->mut_obj().set_poller_id(0);
          for (const auto& poller_id : hgp.pollers())
            poller_ids.insert(poller_id);
          _hostgroups.get<by_id>().insert(
              std::make_pair(hst_grp, std::move(poller_ids)));
          for (uint64_t host_id : hgp.hosts()) {
            _host_hostgroups.insert({host_id, hst_grp});
          }
        }

        for (const auto& sgp : to_load.servicegroups()) {
          auto svc_grp = std::make_shared<neb::pb_service_group>();
          auto poller_ids = absl::flat_hash_set<uint64_t>();
          svc_grp->mut_obj().CopyFrom(sgp.servicegroup());
          /* The poller_id field is not saved in the cache, so we set it to 0
           * here. The actual poller IDs are stored in the poller_ids set, which
           * is populated from the pollers() field of the service group in the
           * cache file. */
          svc_grp->mut_obj().set_poller_id(0);
          for (const auto& poller_id : sgp.pollers())
            poller_ids.insert(poller_id);
          _servicegroups.get<by_id>().insert(
              std::make_pair(svc_grp, std::move(poller_ids)));
          for (const auto& id : sgp.services()) {
            _service_servicegroups.insert(
                {id.host_id(), id.service_id(), svc_grp});
          }
        }
      }  // end of legacy-only heavy sections
      /* Active downtimes are read back in both modes; they are re-injected into
       * the downtime_manager once their host/service is known (now in legacy
       * mode, later via merge() in centralized mode). */
      _pending_active_downtimes.assign(to_load.active_downtimes().begin(),
                                       to_load.active_downtimes().end());
      /* Restore the downtime-comment id counter; ignore 0 / a value below the
       * partition base (old cache file) so we never regress into Engine's
       * range. */
      if (to_load.next_downtime_comment_id() > _downtime_comment_id_base)
        _next_downtime_comment_id.store(to_load.next_downtime_comment_id(),
                                        std::memory_order_relaxed);
      for (const auto& a : to_load.acknowledgements()) {
        auto ack = std::make_shared<neb::pb_acknowledgement>();
        ack->mut_obj().CopyFrom(a);
        _acknowledgements.insert_or_assign({a.host_id(), a.service_id()}, ack);
      }
      SPDLOG_LOGGER_INFO(_logger, "broker_cache: cache loaded from file '{}'",
                         _cache_file.string());
    }
  } else {
    SPDLOG_LOGGER_INFO(_logger, "broker_cache: cache file '{}' does not exist",
                       _cache_file.string());
  }
  /* NOTE: do NOT re-inject here. _load_cache() runs from the broker_cache
   * constructor, and the re-injection path reaches back into
   * config::applier::state::instance().cache() (via the downtime callbacks'
   * resource_exists()), which would assert: _global_cache is not assigned until
   * make_unique<broker_cache> returns. The persisted active downtimes are kept
   * in _pending_active_downtimes and re-injected once _global_cache is set:
   * right after construction (state::initialize_cache, covering legacy mode)
   * and again after each merge() (centralized mode, from
   * _process_engine_state). */
}

/**
 * @brief Save the cache to a file. This is used to persist the cache across
 * restarts of the broker. The cache is saved in a binary format using
 * Protocol Buffers serialization.
 */
void broker_cache::_save_cache() {
  SPDLOG_LOGGER_INFO(_logger, "broker_cache: saving cache...");
  BrokerCache to_save;
  {
    absl::ReaderMutexLock lck{&_mutex};
    /* The heavy cache sections are persisted only in legacy mode; in
     * centralized mode they are rebuilt from the Engine configuration, so only
     * the active downtimes (below) are persisted. */
    if (!config::applier::state::instance().supports_centralized_conf()) {
      for (const auto& [id, info] : _instances) {
        auto* inst_pair = to_save.add_instances();
        inst_pair->set_id(id);
        inst_pair->set_name(info.name);
      }
      for (const auto& host : _hosts) {
        auto* h = to_save.add_hosts();
        h->CopyFrom(host->obj());
      }
      for (const auto& svc : _services) {
        auto* s = to_save.add_services();
        s->CopyFrom(svc->obj());
      }
      absl::flat_hash_map<uint64_t, std::list<uint64_t>> hostgroup_hosts;
      for (const auto& p : _host_hostgroups)
        hostgroup_hosts[p.hostgroup->obj().hostgroup_id()].push_back(p.host_id);
      for (const auto& hg : _hostgroups) {
        auto* hst_grp = to_save.add_hostgroups();
        hst_grp->mutable_hostgroup()->CopyFrom(hg.first->obj());
        for (uint64_t poller_id : hg.second)
          hst_grp->add_pollers(poller_id);
        for (const uint64_t host_id :
             hostgroup_hosts[hg.first->obj().hostgroup_id()])
          hst_grp->add_hosts(host_id);
      }

      absl::flat_hash_map<uint64_t, std::list<std::pair<uint64_t, uint64_t>>>
          servicegroup_services;
      for (const auto& t : _service_servicegroups)
        servicegroup_services[t.servicegroup->obj().servicegroup_id()]
            .emplace_back(std::make_pair(t.host_id, t.service_id));
      for (const auto& hg : _servicegroups) {
        auto* svc_grp = to_save.add_servicegroups();
        svc_grp->mutable_servicegroup()->CopyFrom(hg.first->obj());
        for (uint64_t poller_id : hg.second)
          svc_grp->add_pollers(poller_id);
        for (const auto& [host_id, service_id] :
             servicegroup_services[hg.first->obj().servicegroup_id()]) {
          auto* s = svc_grp->add_services();
          s->set_host_id(host_id);
          s->set_service_id(service_id);
        }
      }
    }  // end of legacy-only heavy sections
    /* Active downtimes are persisted in both legacy and centralized mode so
     * they can be re-injected into the downtime_manager on the next start. */
    for (const auto& d : _active_downtimes_to_save)
      to_save.add_active_downtimes()->CopyFrom(d);
    /* Keep the downtime-comment internal_id counter monotonic across restarts.
     */
    to_save.set_next_downtime_comment_id(
        _next_downtime_comment_id.load(std::memory_order_relaxed));
    /* Open acknowledgements are persisted (both modes) so the cache can still
     * close them on recovery after a restart; the DB rows already exist, this
     * only persists the in-memory tracking. */
    for (const auto& it : _acknowledgements)
      to_save.add_acknowledgements()->CopyFrom(it.second->obj());
  }
  /* Saving the BrokerCache */
  std::ofstream ofs{_cache_file, std::ios::binary | std::ios::trunc};
  if (!ofs) {
    SPDLOG_LOGGER_ERROR(_logger, "broker_cache: cannot open cache file '{}'",
                        _cache_file.string());
  } else {
    if (!to_save.SerializeToOstream(&ofs)) {
      SPDLOG_LOGGER_ERROR(_logger,
                          "broker_cache: cannot serialize cache to file '{}'",
                          _cache_file.string());
    } else {
      SPDLOG_LOGGER_INFO(_logger, "broker_cache: cache saved to file '{}'",
                         _cache_file.string());
    }
  }
}

/**
 * @brief Store the started downtimes to persist on the next cache save. Called
 * by broker_state at shutdown, before the downtime_manager is unloaded.
 */
void broker_cache::set_active_downtimes(std::vector<Downtime> downtimes) {
  absl::WriterMutexLock lck{&_mutex};
  _active_downtimes_to_save = std::move(downtimes);
}

/**
 * @brief Re-inject into the downtime_manager every pending active downtime
 * whose host/service is now known to the cache, then restore its
 * scheduled_downtime_depth.
 *
 * Idempotent and drains _pending_active_downtimes as resources become known
 * (immediately in legacy mode, later via merge() in centralized mode).
 */
void broker_cache::reinject_pending_downtimes() {
  if (!com::centreon::common::downtimes::downtime_manager::is_loaded())
    return;

  /* Snapshot under lock, then process WITHOUT the lock: reload_started_downtime
   * and host()/service() take the cache lock through the downtime callbacks, so
   * holding it here would deadlock. */
  std::vector<Downtime> pending;
  {
    absl::WriterMutexLock lck{&_mutex};
    if (_pending_active_downtimes.empty())
      return;
    pending = _pending_active_downtimes;
  }

  std::vector<Downtime> reinjected;
  std::vector<Downtime> still_pending;
  for (const auto& d : pending) {
    bool exists = d.service_id() == 0
                      ? host(d.host_id()) != nullptr
                      : service(d.host_id(), d.service_id()) != nullptr;
    if (!exists) {
      still_pending.push_back(d);
      continue;
    }
    com::centreon::common::downtimes::downtime_manager::instance()
        .reload_started_downtime(d.host_id(), d.service_id(), d.entry_time(),
                                 d.author(), d.comment_data(), d.start_time(),
                                 d.end_time(), d.fixed(), d.triggered_by(),
                                 d.duration(), d.id(), d.comment_id());
    reinjected.push_back(d);
  }

  {
    absl::WriterMutexLock lck{&_mutex};
    _pending_active_downtimes = std::move(still_pending);
  }

  if (reinjected.empty())
    return;

  /* Restore scheduled_downtime_depth: re-derive it from the count of reloaded
   * active downtimes per resource (idempotent), set it in the cache and publish
   * it so the DB matches (the DB value may have been reset by the Engine
   * configuration that repopulated the cache in centralized mode). */
  absl::flat_hash_map<std::pair<uint64_t, uint64_t>, int32_t> depth;
  for (const auto& d : reinjected)
    ++depth[{d.host_id(), d.service_id()}];

  {
    absl::WriterMutexLock lck{&_mutex};
    for (const auto& [key, cnt] : depth) {
      /* The cached objects are shared with readers that no longer hold the
       * mutex: clone, change the clone, then swap. */
      if (key.second == 0) {
        auto& index = _hosts.get<by_id>();
        auto it = index.find(key.first);
        if (it != index.end()) {
          auto h = std::make_shared<neb::pb_host>(**it);
          h->mut_obj().set_scheduled_downtime_depth(cnt);
          index.replace(it, h);
        }
      } else {
        auto& index = _services.get<by_id>();
        auto it = index.find(key);
        if (it != index.end()) {
          auto s = std::make_shared<neb::pb_service>(**it);
          s->mut_obj().set_scheduled_downtime_depth(cnt);
          index.replace(it, s);
        }
      }
    }
  }

  multiplexing::publisher pblshr;
  for (const auto& [key, cnt] : depth) {
    if (key.second == 0) {
      auto ev = std::make_shared<neb::pb_adaptive_host_status>();
      ev->mut_obj().set_host_id(key.first);
      ev->mut_obj().set_scheduled_downtime_depth(cnt);
      pblshr.write(ev);
    } else {
      auto ev = std::make_shared<neb::pb_adaptive_service_status>();
      ev->mut_obj().set_host_id(key.first);
      ev->mut_obj().set_service_id(key.second);
      ev->mut_obj().set_scheduled_downtime_depth(cnt);
      pblshr.write(ev);
    }
  }

  SPDLOG_LOGGER_INFO(_logger,
                     "broker_cache: re-injected {} active downtime(s) into the "
                     "downtime manager",
                     reinjected.size());
}

/**
 * @brief Insert or update an index mapping in the cache.
 *
 * The mapping is keyed by its index_id: an existing entry is replaced, an
 * unknown one is inserted. No-op when the metric mappings section is disabled.
 *
 * @param index_mapping The index mapping event to store.
 */
void broker_cache::update_index_mapping(
    const std::shared_ptr<storage::pb_index_mapping>& index_mapping) {
  if (!section_enabled(CACHE_METRIC_MAPPINGS))
    return;
  absl::WriterMutexLock l{&_mutex};
  auto& obj = index_mapping->obj();
  auto& index = _index_mappings.get<by_id>();
  auto found = index.find(obj.index_id());
  if (found != index.end())
    index.replace(found, index_mapping);
  else
    index.insert(index_mapping);
}

/**
 * @brief Remove the index mapping of a service from the cache.
 *
 * No-op when the metric mappings section is disabled.
 *
 * @param host_id The host ID of the service.
 * @param service_id The service ID of the service.
 */
void broker_cache::remove_index_mapping(uint64_t host_id, uint64_t service_id) {
  if (!section_enabled(CACHE_METRIC_MAPPINGS))
    return;
  absl::WriterMutexLock l{&_mutex};
  auto& index = _index_mappings.get<by_service>();
  index.erase(std::make_pair(host_id, service_id));
}

}  // namespace com::centreon::broker::cache
