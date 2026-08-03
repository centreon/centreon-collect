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
#ifndef CCB_CACHE_BROKER_CACHE_HH
#define CCB_CACHE_BROKER_CACHE_HH
#include <absl/base/thread_annotations.h>
#include <absl/container/btree_set.h>
#include <absl/container/node_hash_map.h>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index_container.hpp>
#include <chrono>
#include <filesystem>
#include <optional>

#include "com/centreon/broker/bam/internal.hh"
#include "com/centreon/broker/neb/acknowledgement.hh"
#include "com/centreon/broker/neb/internal.hh"
#include "com/centreon/broker/neb/service_status.hh"
#include "common/notifications/escalation.hh"
#include "common/notifications/notification_types.hh"
#include "common/timeperiods/timeperiod.hh"

namespace com::centreon::engine::configuration {
class State;
class Host;
class Service;
class Hostdependency;
class Servicedependency;
class Hostescalation;
class Serviceescalation;
class Contact;
class Contactgroup;
class StringSet;
}  // namespace com::centreon::engine::configuration

namespace com::centreon::broker {

class Host;
class Service;

struct by_id {};
struct by_name {};
struct by_service {};
struct by_instance {};
struct by_severity {};
struct by_dependent {};

namespace cache {
struct host_id_extractor {
  using result_type = uint64_t;
  result_type operator()(const std::shared_ptr<neb::pb_host>& h) const {
    return h->obj().host_id();
  }
};

struct host_name_extractor {
  using result_type = std::string_view;
  std::string_view operator()(const std::shared_ptr<neb::pb_host>& h) const {
    return h->obj().name();
  }
};

struct host_severity_extractor {
  using result_type = uint64_t;
  result_type operator()(const std::shared_ptr<neb::pb_host>& h) const {
    return h->obj().severity_id();
  }
};

struct host_instance_extractor {
  using result_type = uint64_t;
  result_type operator()(const std::shared_ptr<neb::pb_host>& h) const {
    return h->obj().instance_id();
  }
};

using HostContainer = boost::multi_index::multi_index_container<
    std::shared_ptr<neb::pb_host>,
    boost::multi_index::indexed_by<
        boost::multi_index::hashed_unique<boost::multi_index::tag<by_id>,
                                          host_id_extractor>,
        boost::multi_index::hashed_non_unique<
            boost::multi_index::tag<by_instance>,
            host_instance_extractor>,
        boost::multi_index::hashed_non_unique<
            boost::multi_index::tag<by_severity>,
            host_severity_extractor>,
        boost::multi_index::hashed_unique<boost::multi_index::tag<by_name>,
                                          host_name_extractor>>>;

struct service_id_extractor {
  using result_type = std::pair<uint64_t, uint64_t>;
  result_type operator()(const std::shared_ptr<neb::pb_service>& h) const {
    return {h->obj().host_id(), h->obj().service_id()};
  }
};

struct service_severity_extractor {
  using result_type = uint64_t;
  result_type operator()(const std::shared_ptr<neb::pb_service>& h) const {
    return h->obj().severity_id();
  }
};

struct service_name_extractor {
  using result_type = std::pair<std::string_view, std::string_view>;
  result_type operator()(const std::shared_ptr<neb::pb_service>& h) const {
    return {h->obj().host_name(), h->obj().description()};
  }
};

using ServiceContainer = boost::multi_index::multi_index_container<
    std::shared_ptr<neb::pb_service>,
    boost::multi_index::indexed_by<
        boost::multi_index::ordered_unique<boost::multi_index::tag<by_id>,
                                           service_id_extractor>,
        boost::multi_index::hashed_non_unique<
            boost::multi_index::tag<by_severity>,
            service_severity_extractor>,
        boost::multi_index::ordered_unique<boost::multi_index::tag<by_name>,
                                           service_name_extractor>>>;

struct hostgroup_id_extractor {
  using result_type = uint64_t;
  result_type operator()(
      const std::pair<std::shared_ptr<neb::pb_host_group>,
                      absl::flat_hash_set<uint64_t>>& p) const {
    return p.first->obj().hostgroup_id();
  }
};

struct hostgroup_name_extractor {
  using result_type = std::string_view;
  std::string_view operator()(
      const std::pair<std::shared_ptr<neb::pb_host_group>,
                      absl::flat_hash_set<uint64_t>>& p) const {
    return p.first->obj().name();
  }
};

using HostgroupContainer = boost::multi_index::multi_index_container<
    std::pair<std::shared_ptr<neb::pb_host_group>,
              absl::flat_hash_set<uint64_t>>,
    boost::multi_index::indexed_by<
        boost::multi_index::hashed_unique<boost::multi_index::tag<by_id>,
                                          hostgroup_id_extractor>,
        boost::multi_index::hashed_unique<boost::multi_index::tag<by_name>,
                                          hostgroup_name_extractor>>>;

struct by_pair {};
struct by_host {};
struct by_hostgroup {};
struct by_servicegroup {};

struct indexed_host_hostgroup {
  uint64_t host_id;
  std::shared_ptr<neb::pb_host_group> hostgroup;

  indexed_host_hostgroup(uint64_t host_id,
                         std::shared_ptr<neb::pb_host_group> hostgroup)
      : host_id{host_id}, hostgroup{std::move(hostgroup)} {}
};

struct host_hostgroup_pair_extractor {
  using result_type = std::pair<uint64_t, uint64_t>;
  result_type operator()(const indexed_host_hostgroup& hh) const {
    return {hh.host_id, hh.hostgroup->obj().hostgroup_id()};
  }
};

struct host_hostgroup_second_extractor {
  using result_type = uint64_t;
  result_type operator()(const indexed_host_hostgroup& hh) const {
    return hh.hostgroup->obj().hostgroup_id();
  }
};

using HostHostgroupContainer = boost::multi_index::multi_index_container<
    indexed_host_hostgroup,
    boost::multi_index::indexed_by<
        boost::multi_index::ordered_unique<boost::multi_index::tag<by_pair>,
                                           host_hostgroup_pair_extractor>,
        boost::multi_index::ordered_non_unique<
            boost::multi_index::tag<by_host>,
            boost::multi_index::member<indexed_host_hostgroup,
                                       uint64_t,
                                       &indexed_host_hostgroup::host_id>>,
        boost::multi_index::ordered_non_unique<
            boost::multi_index::tag<by_hostgroup>,
            host_hostgroup_second_extractor>>>;

struct indexed_service_servicegroup {
  uint64_t host_id;
  uint64_t service_id;
  std::shared_ptr<neb::pb_service_group> servicegroup;

  indexed_service_servicegroup(
      uint64_t host_id,
      uint64_t service_id,
      std::shared_ptr<neb::pb_service_group> servicegroup)
      : host_id{host_id},
        service_id{service_id},
        servicegroup{std::move(servicegroup)} {}
};

struct service_servicegroup_triplet_extractor {
  using result_type = std::tuple<uint64_t, uint64_t, uint64_t>;
  result_type operator()(const indexed_service_servicegroup& ssg) const {
    return {ssg.host_id, ssg.service_id,
            ssg.servicegroup->obj().servicegroup_id()};
  }
};

struct service_servicegroup_by_service_extractor {
  using result_type = std::pair<uint64_t, uint64_t>;
  result_type operator()(const indexed_service_servicegroup& ssg) const {
    return {ssg.host_id, ssg.service_id};
  }
};

struct service_servicegroup_by_servicegroup_extractor {
  using result_type = uint64_t;
  result_type operator()(const indexed_service_servicegroup& ssg) const {
    return ssg.servicegroup->obj().servicegroup_id();
  }
};

using ServiceServicegroupContainer = boost::multi_index::multi_index_container<
    indexed_service_servicegroup,
    boost::multi_index::indexed_by<
        boost::multi_index::ordered_unique<
            boost::multi_index::tag<by_pair>,
            service_servicegroup_triplet_extractor>,
        boost::multi_index::ordered_non_unique<
            boost::multi_index::tag<by_service>,
            service_servicegroup_by_service_extractor>,
        boost::multi_index::ordered_non_unique<
            boost::multi_index::tag<by_servicegroup>,
            service_servicegroup_by_servicegroup_extractor>>>;

struct servicegroup_id_extractor {
  using result_type = uint64_t;
  result_type operator()(
      const std::pair<std::shared_ptr<neb::pb_service_group>,
                      absl::flat_hash_set<uint64_t>>& p) const {
    return p.first->obj().servicegroup_id();
  }
};

struct servicegroup_name_extractor {
  using result_type = std::string_view;
  std::string_view operator()(
      const std::pair<std::shared_ptr<neb::pb_service_group>,
                      absl::flat_hash_set<uint64_t>>& p) const {
    return p.first->obj().name();
  }
};

using ServicegroupContainer = boost::multi_index::multi_index_container<
    std::pair<std::shared_ptr<neb::pb_service_group>,
              absl::flat_hash_set<uint64_t>>,
    boost::multi_index::indexed_by<
        boost::multi_index::hashed_unique<boost::multi_index::tag<by_id>,
                                          servicegroup_id_extractor>,
        boost::multi_index::hashed_unique<boost::multi_index::tag<by_name>,
                                          servicegroup_name_extractor>>>;

struct indexmapping_index_id_extractor {
  using result_type = uint64_t;
  result_type operator()(
      const std::shared_ptr<storage::pb_index_mapping>& info) const {
    return info->obj().index_id();
  }
};

struct indexmapping_service_id_extractor {
  using result_type = std::pair<uint64_t, uint64_t>;
  result_type operator()(
      const std::shared_ptr<storage::pb_index_mapping>& info) const {
    const auto& obj = info->obj();
    return std::make_pair(obj.host_id(), obj.service_id());
  }
};

using IndexMappingContainer = boost::multi_index::multi_index_container<
    std::shared_ptr<storage::pb_index_mapping>,
    boost::multi_index::indexed_by<
        boost::multi_index::hashed_unique<boost::multi_index::tag<by_id>,
                                          indexmapping_index_id_extractor>,
        boost::multi_index::hashed_unique<boost::multi_index::tag<by_service>,
                                          indexmapping_service_id_extractor>>>;

/* Notification-only dependency of one host on another, mirrored from the Engine
 * configuration (post-expand: a single dependent host and a single master
 * host, resolved to ids). `key` is the engine_conf hostdependency_key() hash of
 * the source dependency; it is kept so an incremental DiffState can erase an
 * entry by the key its `removed` list carries. Indexed by dependent host id
 * (the evaluation lookup) and by poller id (bulk purge on reconfiguration or
 * disconnection). */
struct host_notif_dep {
  uint64_t dependent_host_id;
  uint64_t master_host_id;
  uint64_t poller_id;
  std::string dependency_period;
  bool inherits_parent;
  uint32_t notification_failure_options;
  size_t key;
};

struct host_notif_dep_dependent_extractor {
  using result_type = uint64_t;
  result_type operator()(const host_notif_dep& d) const {
    return d.dependent_host_id;
  }
};

using HostNotifDepContainer = boost::multi_index::multi_index_container<
    host_notif_dep,
    boost::multi_index::indexed_by<
        boost::multi_index::hashed_non_unique<
            boost::multi_index::tag<by_dependent>,
            host_notif_dep_dependent_extractor>,
        boost::multi_index::hashed_non_unique<
            boost::multi_index::tag<by_instance>,
            boost::multi_index::
                member<host_notif_dep, uint64_t, &host_notif_dep::poller_id>>>>;

struct service_notif_dep {
  uint64_t dependent_host_id;
  uint64_t dependent_service_id;
  uint64_t master_host_id;
  uint64_t master_service_id;
  uint64_t poller_id;
  std::string dependency_period;
  bool inherits_parent;
  uint32_t notification_failure_options;
  size_t key;
};

struct service_notif_dep_dependent_extractor {
  using result_type = std::pair<uint64_t, uint64_t>;
  result_type operator()(const service_notif_dep& d) const {
    return {d.dependent_host_id, d.dependent_service_id};
  }
};

using ServiceNotifDepContainer = boost::multi_index::multi_index_container<
    service_notif_dep,
    boost::multi_index::indexed_by<
        boost::multi_index::hashed_non_unique<
            boost::multi_index::tag<by_dependent>,
            service_notif_dep_dependent_extractor>,
        boost::multi_index::hashed_non_unique<
            boost::multi_index::tag<by_instance>,
            boost::multi_index::member<service_notif_dep,
                                       uint64_t,
                                       &service_notif_dep::poller_id>>>>;

/* Notification escalation of a host, mirrored from the Engine configuration
 * (post-expand: a single host, resolved to its id). `escalate_on` is stored
 * already converted from the config escalation_options bitmask to the
 * notification_flag bitmask used at evaluation time. The contactgroups are kept
 * by name and expanded to member contacts at query time, as done for the
 * resource's direct contactgroups. `key` is the engine_conf
 * hostescalation_key() hash, kept so an incremental DiffState can erase an
 * entry by the key its `removed` list carries. Indexed by host id (the
 * evaluation lookup) and by poller id (bulk purge on reconfiguration or
 * disconnection). */
struct host_escalation {
  uint64_t host_id;
  uint64_t poller_id;
  uint32_t first_notification;
  uint32_t last_notification;
  uint32_t notification_interval;
  std::string escalation_period;
  uint32_t escalate_on;
  std::vector<std::string> contactgroups;
  size_t key;
};

struct host_escalation_host_extractor {
  using result_type = uint64_t;
  result_type operator()(const host_escalation& e) const { return e.host_id; }
};

using HostEscalationContainer = boost::multi_index::multi_index_container<
    host_escalation,
    boost::multi_index::indexed_by<
        boost::multi_index::hashed_non_unique<boost::multi_index::tag<by_id>,
                                              host_escalation_host_extractor>,
        boost::multi_index::hashed_non_unique<
            boost::multi_index::tag<by_instance>,
            boost::multi_index::member<host_escalation,
                                       uint64_t,
                                       &host_escalation::poller_id>>>>;

struct service_escalation {
  uint64_t host_id;
  uint64_t service_id;
  uint64_t poller_id;
  uint32_t first_notification;
  uint32_t last_notification;
  uint32_t notification_interval;
  std::string escalation_period;
  uint32_t escalate_on;
  std::vector<std::string> contactgroups;
  size_t key;
};

struct service_escalation_service_extractor {
  using result_type = std::pair<uint64_t, uint64_t>;
  result_type operator()(const service_escalation& e) const {
    return {e.host_id, e.service_id};
  }
};

using ServiceEscalationContainer = boost::multi_index::multi_index_container<
    service_escalation,
    boost::multi_index::indexed_by<
        boost::multi_index::hashed_non_unique<
            boost::multi_index::tag<by_id>,
            service_escalation_service_extractor>,
        boost::multi_index::hashed_non_unique<
            boost::multi_index::tag<by_instance>,
            boost::multi_index::member<service_escalation,
                                       uint64_t,
                                       &service_escalation::poller_id>>>>;

class broker_cache {
 public:
  struct severity {
    uint32_t level;
    /* This db_id is the ID of the severity in the database. It is initialized
     * to 0 when the severity is not yet stored in the database, and updated
     * when done.
     */
    uint64_t db_id;
  };

  /* Per-poller information kept in the instances cache. Beside the poller name,
   * it carries the few program-wide Engine settings Broker needs to answer for
   * the poller (e.g. when computing notifications on Broker's side). */
  struct instance_info {
    /* Duration of one Engine "interval unit", used to convert the interval
     * counts of the poller's configuration into absolute durations. */
    static constexpr std::chrono::seconds default_interval_length{60};

    std::string name;
    bool notifications_enabled = true;
    bool send_recovery_notifications_anyway = false;
    std::chrono::seconds interval_length = default_interval_length;
    /* Program-wide Engine flag: when false, a soft master state is read as its
     * last hard state while evaluating notification dependencies. */
    bool soft_state_dependencies = false;
  };

  /* Cache view of a notification contact. This is the shared
   * notification-library snapshot
   * (com::centreon::common::notifications::contact): the same value the
   * viability logic (should_notify_contact) reasons on, so no conversion is
   * needed at decision time. The contactgroups a contact belongs to are NOT
   * stored on it: the contactgroup->contacts resolution is done lazily at
   * notification time. */
  using contact = com::centreon::common::notifications::contact;

  /* Cache view of a contactgroup: its name and the names of its member
   * contacts. The Engine flattens nested contactgroups (contactgroup_members)
   * into `members` before the configuration reaches Broker (post-expand
   * State), so `members` already holds only concrete contact names. */
  struct contactgroup {
    std::string name;
    absl::flat_hash_set<std::string> members;
  };

  /* The direct contacts and contactgroups a resource (host or service)
   * notifies. To avoid duplicating the names (already owned once by _contacts /
   * _contactgroups), the resource holds non-owning pointers into those maps.
   * This is safe because _contacts / _contactgroups are node_hash_maps (stable
   * element addresses across rehash) and, feeding being ordered (contacts
   * before resources) and done entirely under the write lock, a pointer is
   * never observed dangling. Kept separate (not merged), as done by Engine: the
   * contactgroup->contacts expansion is done at query time. `poller_id` is
   * carried so the whole per-poller set can be purged on reconfiguration or
   * disconnection. */
  struct resource_contacts {
    std::vector<const contact*> contacts;
    std::vector<const contactgroup*> contactgroups;
    uint64_t poller_id = 0;
  };

  /* Result of the escalation evaluation for a resource (see
   * notification_escalation()). `escalated` is true as soon as one escalation
   * is viable for the current (state, notification_number, time); in that case
   * `notification_interval` is the smallest interval among the viable
   * escalations (raw config units, to be multiplied by the poller
   * interval_length) and `contact_names` is the union of the member contacts of
   * their contactgroups. When no escalation is viable, `escalated` is false and
   * the caller falls back to the resource's direct contacts (iso Engine
   * get_contacts_to_notify). */
  struct escalation_result {
    bool escalated = false;
    uint32_t notification_interval = 0;
    absl::flat_hash_set<std::string> contact_names;
  };

  enum cache_section : uint32_t {
    CACHE_NONE = 0,
    CACHE_INSTANCES = 1 << 0,
    CACHE_HOSTS = 1 << 1,
    CACHE_SERVICES = 1 << 2,
    CACHE_GROUPS = 1 << 3,
    CACHE_METRIC_MAPPINGS = 1 << 4,
    CACHE_SEVERITIES = 1 << 5,
    CACHE_BAM = 1 << 6,
    CACHE_TAGS = 1 << 7,
    /* Notification-only data (notification timeperiods and notification
     * dependencies): stored only when Broker computes notifications on its
     * side. Enabling it implies CACHE_INSTANCES | CACHE_HOSTS | CACHE_SERVICES
     * (see enable_section()), which the notification path needs to resolve the
     * dependency names to ids, look up the poller of a resource and read the
     * per-poller notification settings. */
    CACHE_NOTIFICATIONS = 1 << 8,
    CACHE_ALL = 0xFFFFFFFF,
  };

 private:
  std::shared_ptr<spdlog::logger> _logger;
  /**
   * @brief Absolute path of the on-disk cache file for this instance.
   *
   * Computed once in the constructor so that _load_cache() and _save_cache()
   * always use the same path.  When cache_dir() is empty (unit-test context),
   * a per-process, per-instance unique path under /tmp is used so that
   * concurrent processes and successive deinit/init cycles within the same
   * process never share the same file.
   */
  std::filesystem::path _cache_file;
  std::atomic<uint32_t> _enabled_sections{CACHE_NONE};

  mutable absl::Mutex _mutex;
  absl::flat_hash_map<uint64_t, instance_info> _instances
      ABSL_GUARDED_BY(_mutex);

  HostContainer _hosts ABSL_GUARDED_BY(_mutex);
  ServiceContainer _services ABSL_GUARDED_BY(_mutex);
  /* The host groups cache stores also a set with the pollers telling they need
   * the cache. So if no more poller needs a host group, we can remove it from
   * the cache. */
  HostgroupContainer _hostgroups ABSL_GUARDED_BY(_mutex);
  /* Association between hosts and hostgroups. The first of the pair is the
   * host_id, the second the hostgroup_id. We can get them by {host_id, hg_id},
   * or only by host_id or only by hostgroup_id. */
  HostHostgroupContainer _host_hostgroups ABSL_GUARDED_BY(_mutex);
  /* The service groups cache stores also a set with the pollers telling they
   * need the cache. So if no more poller needs a service group, we can remove
   * it from the cache. */
  ServicegroupContainer _servicegroups ABSL_GUARDED_BY(_mutex);
  /* Association between services and servicegroups. The first two values are
   * host_id:service_id of the service and the last is the servicegroup ID. */
  ServiceServicegroupContainer _service_servicegroups ABSL_GUARDED_BY(_mutex);

  /* Notification-only host/service dependencies, keyed by dependent id and by
   * poller. Only fed when CACHE_NOTIFICATIONS is enabled (which also pulls in
   * CACHE_HOSTS / CACHE_SERVICES, needed to resolve the referenced names to
   * ids). */
  HostNotifDepContainer _host_notif_deps ABSL_GUARDED_BY(_mutex);
  ServiceNotifDepContainer _service_notif_deps ABSL_GUARDED_BY(_mutex);

  /* Notification-only host/service escalations, keyed by resource id and by
   * poller. Fed alongside the dependencies (same CACHE_NOTIFICATIONS gate); the
   * referenced contactgroups live in _contactgroups. */
  HostEscalationContainer _host_escalations ABSL_GUARDED_BY(_mutex);
  ServiceEscalationContainer _service_escalations ABSL_GUARDED_BY(_mutex);

  /* Notification contacts and contactgroups, keyed by name. Each value pairs
   * the cached object with the set of pollers referencing it (same "central,
   * reference-counted" model as _tags / _severities): an entry is dropped only
   * when no poller references it anymore. Only fed when CACHE_NOTIFICATIONS is
   * enabled. Contactgroups are kept unflattened onto the contacts; the
   * contactgroup->contacts expansion is done at query time (see
   * contactgroup_members()).
   *
   * node_hash_map (not flat) is mandatory here: _resource_contacts holds raw
   * pointers into these values, so element addresses must stay stable across
   * rehash. For the same reason a modification updates the object in place
   * (assigns into it->second.first); it never rebinds the map entry. */
  absl::node_hash_map<std::string,
                      std::pair<contact, absl::flat_hash_set<uint64_t>>>
      _contacts ABSL_GUARDED_BY(_mutex);
  absl::node_hash_map<std::string,
                      std::pair<contactgroup, absl::flat_hash_set<uint64_t>>>
      _contactgroups ABSL_GUARDED_BY(_mutex);

  /* The direct contacts/contactgroups of each resource, keyed by
   * {host_id, service_id} (service_id == 0 designates a host). Fed alongside
   * the hosts/services when CACHE_NOTIFICATIONS is enabled; resolved to
   * concrete contact names at query time (see notification_contact_names()). */
  absl::flat_hash_map<std::pair<uint64_t, uint64_t>, resource_contacts>
      _resource_contacts ABSL_GUARDED_BY(_mutex);

  IndexMappingContainer _index_mappings ABSL_GUARDED_BY(_mutex);
  absl::flat_hash_map<std::pair<uint64_t, uint64_t>,
                      std::shared_ptr<storage::pb_index_mapping>>
      _index_cache ABSL_GUARDED_BY(_mutex);

  absl::flat_hash_map<uint64_t, std::shared_ptr<storage::pb_metric_mapping>>
      _metric_mappings ABSL_GUARDED_BY(_mutex);

  /* Key for severities is {severity_id, severity_type},
   * value is the struct severity defined earlier (fields are level and ID) */
  absl::flat_hash_map<std::pair<uint64_t, uint32_t>,
                      std::pair<severity, absl::flat_hash_set<uint64_t>>>
      _severities ABSL_GUARDED_BY(_mutex);

  /* Key for tags is {tag_id, tag_type}.
   * Value is the pb_tag plus the set of poller IDs that define this tag.
   * The tag is removed only when no poller references it anymore. */
  absl::flat_hash_map<
      std::pair<uint64_t, TagType>,
      std::pair<std::shared_ptr<neb::pb_tag>, absl::flat_hash_set<uint64_t>>>
      _tags ABSL_GUARDED_BY(_mutex);

  /* Notification timeperiods, owned by the cache and keyed by name */
  absl::flat_hash_map<std::string,
                      std::shared_ptr<common::timeperiods::timeperiod>>
      _timeperiods ABSL_GUARDED_BY(_mutex);
  /* Which pollers reference each timeperiod name. A timeperiod is dropped from
   * _timeperiods only when no poller references it anymore (same "central,
   * reference-counted" model as _tags / _severities / groups). */
  absl::flat_hash_map<std::string, absl::flat_hash_set<uint64_t>>
      _timeperiod_pollers ABSL_GUARDED_BY(_mutex);

  /* Anomaly detection index: maps {host_id, dependent_service_id} to the set
   * of anomaly detection service IDs that monitor that dependent service.
   * Only anomaly detection services appear here. */
  absl::flat_hash_map<std::pair<uint64_t, uint64_t>,
                      absl::flat_hash_set<uint64_t>>
      _anomaly_detection_index ABSL_GUARDED_BY(_mutex);

  /* BAM relations from BA to BV */
  absl::btree_set<std::pair<uint64_t, uint64_t>> _dimension_ba_bv_relations
      ABSL_GUARDED_BY(_mutex);

  absl::flat_hash_map<uint64_t, std::shared_ptr<bam::pb_dimension_bv_event>>
      _dimension_bvs ABSL_GUARDED_BY(_mutex);

  absl::flat_hash_map<uint64_t, std::shared_ptr<bam::pb_dimension_ba_event>>
      _dimension_bas ABSL_GUARDED_BY(_mutex);

  /* Acknowledgements list. */
  absl::flat_hash_map<std::pair<uint64_t, uint64_t>,
                      std::shared_ptr<neb::pb_acknowledgement>>
      _acknowledgements ABSL_GUARDED_BY(_mutex);

  /* Active (started) downtimes to persist with the cache (set by broker_state
   * just before the downtime_manager is unloaded). */
  std::vector<Downtime> _active_downtimes_to_save ABSL_GUARDED_BY(_mutex);
  /* Active downtimes read back from the cache file, awaiting re-injection into
   * the downtime_manager once their host/service is known to the cache. */
  std::vector<Downtime> _pending_active_downtimes ABSL_GUARDED_BY(_mutex);

  /* Per-resource notification runtime state to persist with the cache (set by
   * broker_state just before the notification_manager is unloaded). */
  std::vector<BrokerCache::NotificationState> _notification_states_to_save
      ABSL_GUARDED_BY(_mutex);
  /* Notification states read back from the cache file, awaiting re-injection
   * into the notification_manager on the next start. */
  std::vector<BrokerCache::NotificationState> _pending_notification_states
      ABSL_GUARDED_BY(_mutex);

  /* INT32_MAX/2: base of the partition reserved for Broker-originated downtime
   * comment internal_ids, disjoint from Engine's per-poller ids (which start at
   * 1). The comments.internal_id column is a signed int(11), so this stays
   * positive and leaves ~1.07e9 ids of headroom. */
  static constexpr uint64_t _downtime_comment_id_base = 0x3FFFFFFF;
  /* Monotonic counter for those internal_ids. Atomic (minted on io_context
   * threads) and persisted with the cache so it stays monotonic across a cbd
   * restart. */
  std::atomic<uint64_t> _next_downtime_comment_id{_downtime_comment_id_base};

  void _fill_host(Host* host,
                  const com::centreon::engine::configuration::Host& cfg,
                  uint64_t poller_id_hint = 0)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(_mutex);
  template <typename ConfigType>
  void _fill_service_common(Service* obj, const ConfigType& cfg);
  void _fill_service(Service* service,
                     const com::centreon::engine::configuration::Service& cfg)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(_mutex);
  void _fill_anomaly_detection(
      Service* service,
      const com::centreon::engine::configuration::Anomalydetection& cfg)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(_mutex);
  bool _replace_hostgroup(
      HostgroupContainer::index<by_id>::type::iterator found,
      const std::shared_ptr<neb::pb_host_group>& replacement)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(_mutex);
  bool _replace_servicegroup(
      ServicegroupContainer::index<by_id>::type::iterator found,
      const std::shared_ptr<neb::pb_service_group>& replacement)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(_mutex);
  std::shared_ptr<neb::pb_acknowledgement> _take_expired_acknowledgement(
      uint64_t host_id,
      uint64_t service_id,
      AckType ack_type,
      uint16_t state) ABSL_EXCLUSIVE_LOCKS_REQUIRED(_mutex);
  void _insert_host_notif_dep(
      const com::centreon::engine::configuration::Hostdependency& dep,
      uint64_t poller_id) ABSL_EXCLUSIVE_LOCKS_REQUIRED(_mutex);
  void _insert_service_notif_dep(
      const com::centreon::engine::configuration::Servicedependency& dep,
      uint64_t poller_id) ABSL_EXCLUSIVE_LOCKS_REQUIRED(_mutex);
  void _insert_host_escalation(
      const com::centreon::engine::configuration::Hostescalation& esc,
      uint64_t poller_id) ABSL_EXCLUSIVE_LOCKS_REQUIRED(_mutex);
  void _insert_service_escalation(
      const com::centreon::engine::configuration::Serviceescalation& esc,
      uint64_t poller_id) ABSL_EXCLUSIVE_LOCKS_REQUIRED(_mutex);
  void _insert_contact(const com::centreon::engine::configuration::Contact& c,
                       uint64_t poller_id)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(_mutex);
  void _insert_contactgroup(
      const com::centreon::engine::configuration::Contactgroup& cg,
      uint64_t poller_id) ABSL_EXCLUSIVE_LOCKS_REQUIRED(_mutex);
  /* Store (or, when the resource has neither contact nor contactgroup, erase)
   * the direct contacts/contactgroups of a resource. service_id == 0 designates
   * a host. */
  void _insert_resource_contacts(
      uint64_t host_id,
      uint64_t service_id,
      const com::centreon::engine::configuration::StringSet& contacts,
      const com::centreon::engine::configuration::StringSet& contactgroups,
      uint64_t poller_id) ABSL_EXCLUSIVE_LOCKS_REQUIRED(_mutex);
  /* Drop @p poller_id from every contact/contactgroup reference set and from
   * the per-resource contacts, erasing the entries no poller references
   * anymore. Used on a full re-merge (rebuild the poller's contribution) and on
   * poller removal. */
  void _remove_poller_from_contacts(uint64_t poller_id)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(_mutex);
  bool _host_notification_authorized_by_dependencies(uint64_t host_id,
                                                     std::time_t now) const
      ABSL_SHARED_LOCKS_REQUIRED(_mutex);
  bool _service_notification_authorized_by_dependencies(uint64_t host_id,
                                                        uint64_t service_id,
                                                        std::time_t now) const
      ABSL_SHARED_LOCKS_REQUIRED(_mutex);
  void _publish(const std::shared_ptr<io::data>& to_publish)
      ABSL_LOCKS_EXCLUDED(_mutex);
  void _load_cache() ABSL_LOCKS_EXCLUDED(_mutex);
  void _save_cache() ABSL_LOCKS_EXCLUDED(_mutex);
  void _resolve_timeperiods() ABSL_EXCLUSIVE_LOCKS_REQUIRED(_mutex);

 public:
  broker_cache(std::shared_ptr<spdlog::logger> logger);
  broker_cache(const broker_cache&) = delete;
  broker_cache& operator=(const broker_cache&) = delete;
  ~broker_cache() noexcept;

  void enable_section(uint32_t sections) noexcept {
    /* Notification data is meaningless without the instances/hosts/services it
     * refers to, so requesting it pulls those sections in as well. */
    if (sections & CACHE_NOTIFICATIONS)
      sections |= CACHE_INSTANCES | CACHE_HOSTS | CACHE_SERVICES;
    _enabled_sections.fetch_or(sections, std::memory_order_relaxed);
  }
  bool section_enabled(uint32_t section) const noexcept {
    return (_enabled_sections.load(std::memory_order_relaxed) & section) != 0;
  }
  void merge(const com::centreon::engine::configuration::State& state)
      ABSL_LOCKS_EXCLUDED(_mutex);
  /* Store the started downtimes to persist on the next cache save. Called by
   * broker_state at shutdown, before the downtime_manager is unloaded. */
  void set_active_downtimes(std::vector<Downtime> downtimes)
      ABSL_LOCKS_EXCLUDED(_mutex);
  /* Return the next internal_id for a Broker-originated downtime comment and
   * advance the partitioned counter. Thread-safe. */
  uint64_t next_downtime_comment_id() noexcept {
    return _next_downtime_comment_id.fetch_add(1, std::memory_order_relaxed);
  }
  /* Re-inject into the downtime_manager every pending active downtime whose
   * host/service is now known to the cache, restoring its scheduled depth.
   * Idempotent; drains _pending_active_downtimes as resources become known
   * (called after _load_cache in legacy mode and after merge() — via
   * _process_engine_state — in centralized mode). */
  void reinject_pending_downtimes() ABSL_LOCKS_EXCLUDED(_mutex);
  /* Store the per-resource notification states to persist on the next cache
   * save. Called by broker_state at shutdown, before the notification_manager is
   * unloaded. */
  void set_notification_states(
      std::vector<BrokerCache::NotificationState> states)
      ABSL_LOCKS_EXCLUDED(_mutex);
  /* Re-inject the pending notification states into the notification_manager,
   * restoring the notification chain (number, timings, notified contacts) after
   * a restart. Drains _pending_notification_states; a no-op if the manager is
   * not loaded (notification_mode != broker). */
  void reinject_pending_notification_states() ABSL_LOCKS_EXCLUDED(_mutex);
  void apply(const com::centreon::engine::configuration::DiffState& diff)
      ABSL_LOCKS_EXCLUDED(_mutex);
  void update_instance(const std::shared_ptr<neb::pb_instance>& instance)
      ABSL_LOCKS_EXCLUDED(_mutex);
  void update_hostgroup(const std::shared_ptr<neb::pb_host_group>& group)
      ABSL_LOCKS_EXCLUDED(_mutex);
  void update_servicegroup(const std::shared_ptr<neb::pb_service_group>& group)
      ABSL_LOCKS_EXCLUDED(_mutex);
  void update_servicegroup_member(
      const std::shared_ptr<neb::pb_service_group_member>& servicegroup_member)
      ABSL_LOCKS_EXCLUDED(_mutex);
  void update_hostgroup_member(
      const std::shared_ptr<neb::pb_host_group_member>& hostgroup_member)
      ABSL_LOCKS_EXCLUDED(_mutex);
  void update_metric_mapping(
      const std::shared_ptr<storage::pb_metric_mapping>& mm)
      ABSL_LOCKS_EXCLUDED(_mutex);
  void update_host(const std::shared_ptr<neb::pb_host>& host)
      ABSL_LOCKS_EXCLUDED(_mutex);
  void update_host(const std::shared_ptr<neb::pb_host_status>& status)
      ABSL_LOCKS_EXCLUDED(_mutex);
  void update_host(const std::shared_ptr<neb::pb_adaptive_host>& host)
      ABSL_LOCKS_EXCLUDED(_mutex);
  void update_host(const std::shared_ptr<neb::pb_adaptive_host_status>& status)
      ABSL_LOCKS_EXCLUDED(_mutex);
  void update_service(const std::shared_ptr<neb::pb_service>& service)
      ABSL_LOCKS_EXCLUDED(_mutex);
  void update_service(const std::shared_ptr<neb::pb_service_status>& status)
      ABSL_LOCKS_EXCLUDED(_mutex);
  void update_service(const std::shared_ptr<neb::pb_adaptive_service>& service)
      ABSL_LOCKS_EXCLUDED(_mutex);
  void update_service(
      const std::shared_ptr<neb::pb_adaptive_service_status>& status)
      ABSL_LOCKS_EXCLUDED(_mutex);
  std::string instance(uint64_t instance_id) const ABSL_LOCKS_EXCLUDED(_mutex);
  bool notifications_enabled(uint64_t instance_id) const
      ABSL_LOCKS_EXCLUDED(_mutex);
  bool send_recovery_notifications_anyway(uint64_t instance_id) const
      ABSL_LOCKS_EXCLUDED(_mutex);
  bool soft_state_dependencies(uint64_t instance_id) const
      ABSL_LOCKS_EXCLUDED(_mutex);
  std::vector<host_notif_dep> host_notif_dependencies(
      uint64_t dependent_host_id) const ABSL_LOCKS_EXCLUDED(_mutex);
  std::vector<service_notif_dep> service_notif_dependencies(
      uint64_t dependent_host_id,
      uint64_t dependent_service_id) const ABSL_LOCKS_EXCLUDED(_mutex);
  bool notification_authorized_by_dependencies(uint64_t host_id,
                                               uint64_t service_id) const
      ABSL_LOCKS_EXCLUDED(_mutex);
  escalation_result notification_escalation(uint64_t host_id,
                                            uint64_t service_id,
                                            int state,
                                            uint32_t notification_number,
                                            const std::string& timezone,
                                            std::time_t now) const
      ABSL_LOCKS_EXCLUDED(_mutex);
  /* Look up a cached notification contact by name. Returns std::nullopt when no
   * contact of that name is known to the cache. */
  std::optional<contact> contact_config(const std::string& name) const
      ABSL_LOCKS_EXCLUDED(_mutex);
  /* Return the member contact names of a cached contactgroup (empty when the
   * group is unknown or has no member). This is the lazy contactgroup->contacts
   * expansion performed at notification time. */
  std::vector<std::string> contactgroup_members(const std::string& name) const
      ABSL_LOCKS_EXCLUDED(_mutex);
  /* Resolve the notification contacts of a resource ({host_id, service_id};
   * service_id == 0 designates a host): the set of its direct contacts and the
   * members of each of its contactgroups (a set, so naturally deduplicated).
   * This is the query-time contactgroup->contacts expansion (iso Engine);
   * per-contact runtime filtering (should_notify_contact) is left to the
   * caller. Empty when the resource is unknown or references no contact. */
  absl::flat_hash_set<std::string> notification_contact_names(
      uint64_t host_id,
      uint64_t service_id) const ABSL_LOCKS_EXCLUDED(_mutex);
  std::chrono::seconds interval_length(uint64_t instance_id) const
      ABSL_LOCKS_EXCLUDED(_mutex);
  bool in_notification_period(const std::string& period_name,
                              const std::string& timezone,
                              std::time_t now) const
      ABSL_LOCKS_EXCLUDED(_mutex);
  void remove_instance(uint64_t instance_id) ABSL_LOCKS_EXCLUDED(_mutex);
  std::shared_ptr<neb::pb_host> host(const std::string& host_name) const
      ABSL_LOCKS_EXCLUDED(_mutex);
  std::shared_ptr<neb::pb_host> host(uint64_t host_id) const
      ABSL_LOCKS_EXCLUDED(_mutex);
  std::vector<uint64_t> host_ids() const ABSL_LOCKS_EXCLUDED(_mutex);
  std::shared_ptr<neb::pb_service> service(const std::string& hostname,
                                           const std::string& description) const
      ABSL_LOCKS_EXCLUDED(_mutex);
  std::shared_ptr<neb::pb_service> service(uint64_t host_id,
                                           uint64_t service_id) const
      ABSL_LOCKS_EXCLUDED(_mutex);
  std::shared_ptr<storage::pb_index_mapping> get_index_mapping(
      uint64_t host_id,
      uint64_t service_id) const ABSL_LOCKS_EXCLUDED(_mutex);
  std::shared_ptr<storage::pb_index_mapping> get_index_mapping(
      uint64_t index_id) const ABSL_LOCKS_EXCLUDED(_mutex);
  std::shared_ptr<storage::pb_metric_mapping> get_metric_mapping(
      uint64_t metric_id) const ABSL_LOCKS_EXCLUDED(_mutex);
  std::vector<std::pair<uint64_t, uint64_t>> service_ids() const
      ABSL_LOCKS_EXCLUDED(_mutex);
  std::vector<uint64_t> find_anomaly_detection_ids_by_dependent_service(
      uint64_t host_id,
      uint64_t dependent_service_id) const ABSL_LOCKS_EXCLUDED(_mutex);
  uint32_t first_active_instance_id() const ABSL_LOCKS_EXCLUDED(_mutex);
  std::string instance_name(uint64_t poller_id) const
      ABSL_LOCKS_EXCLUDED(_mutex);
  std::vector<uint64_t> service_ids_for_host(uint64_t host_id) const
      ABSL_LOCKS_EXCLUDED(_mutex);
  int32_t add_downtime(uint64_t host_id, uint64_t service_id)
      ABSL_LOCKS_EXCLUDED(_mutex);
  int32_t remove_downtime(uint64_t host_id, uint64_t service_id)
      ABSL_LOCKS_EXCLUDED(_mutex);
  std::shared_ptr<neb::pb_host_group> hostgroup(uint64_t hostgroup_id) const
      ABSL_LOCKS_EXCLUDED(_mutex);
  std::shared_ptr<neb::pb_host_group> hostgroup(const std::string& name) const
      ABSL_LOCKS_EXCLUDED(_mutex);
  std::vector<uint64_t> hostgroup_ids() const ABSL_LOCKS_EXCLUDED(_mutex);
  std::shared_ptr<neb::pb_service_group> servicegroup(
      uint64_t servicegroup_id) const ABSL_LOCKS_EXCLUDED(_mutex);
  std::vector<uint64_t> servicegroup_ids() const ABSL_LOCKS_EXCLUDED(_mutex);
  std::vector<std::shared_ptr<neb::pb_host_group>> hostgroups(
      uint64_t host_id) const ABSL_LOCKS_EXCLUDED(_mutex);
  std::vector<uint64_t> hostgroup_members(uint64_t hostgroup_id) const
      ABSL_LOCKS_EXCLUDED(_mutex);
  std::vector<std::pair<uint64_t, uint64_t>> servicegroup_members(
      uint64_t servicegroup_id) const ABSL_LOCKS_EXCLUDED(_mutex);
  std::vector<std::shared_ptr<neb::pb_service_group>> servicegroups(
      uint64_t host_id,
      uint64_t service_id) const ABSL_LOCKS_EXCLUDED(_mutex);
  void publish(const std::shared_ptr<io::data>& to_publish)
      ABSL_LOCKS_EXCLUDED(_mutex);
  void publish(const std::deque<std::shared_ptr<io::data>>& to_publish)
      ABSL_LOCKS_EXCLUDED(_mutex);
  uint32_t severity(uint64_t host_id, uint64_t service_id) const
      ABSL_LOCKS_EXCLUDED(_mutex);
  void update_severity(const std::shared_ptr<neb::pb_severity>& evt)
      ABSL_LOCKS_EXCLUDED(_mutex);
  void update_tag(const std::shared_ptr<neb::pb_tag>& evt)
      ABSL_LOCKS_EXCLUDED(_mutex);
  std::shared_ptr<neb::pb_tag> get_tag(uint64_t tag_id, TagType type) const
      ABSL_LOCKS_EXCLUDED(_mutex);
  std::vector<uint64_t> host_tag_ids(uint64_t host_id, TagType type) const
      ABSL_LOCKS_EXCLUDED(_mutex);
  std::vector<std::string> host_tag_names(uint64_t host_id, TagType type) const
      ABSL_LOCKS_EXCLUDED(_mutex);
  std::vector<uint64_t> service_tag_ids(uint64_t host_id,
                                        uint64_t service_id,
                                        TagType type) const
      ABSL_LOCKS_EXCLUDED(_mutex);
  std::vector<std::string> service_tag_names(uint64_t host_id,
                                             uint64_t service_id,
                                             TagType type) const
      ABSL_LOCKS_EXCLUDED(_mutex);
  void set_db_id_for_severity(uint64_t config_id, uint32_t type, uint64_t db_id)
      ABSL_LOCKS_EXCLUDED(_mutex);
  void erase_severity(uint64_t config_id, uint32_t type)
      ABSL_LOCKS_EXCLUDED(_mutex);
  bool has_severity(uint64_t config_id, uint32_t type) const
      ABSL_LOCKS_EXCLUDED(_mutex);
  uint64_t get_db_id_for_severity(uint64_t severity_id, uint32_t type)
      ABSL_LOCKS_EXCLUDED(_mutex);
  absl::flat_hash_map<std::pair<uint64_t, uint32_t>, struct severity>
  severities() const ABSL_LOCKS_EXCLUDED(_mutex);
  absl::flat_hash_map<
      std::pair<uint64_t, TagType>,
      std::pair<std::shared_ptr<neb::pb_tag>, absl::flat_hash_set<uint64_t>>>
  tags() const ABSL_LOCKS_EXCLUDED(_mutex);
  std::vector<std::shared_ptr<neb::pb_acknowledgement>> acknowledgements() const
      ABSL_LOCKS_EXCLUDED(_mutex);
  void update_dimension_ba_bv_relation(
      const std::shared_ptr<bam::pb_dimension_ba_bv_relation_event>& rel)
      ABSL_LOCKS_EXCLUDED(_mutex);
  std::vector<uint64_t> dimension_bvs_for_ba(uint64_t ba_id) const
      ABSL_LOCKS_EXCLUDED(_mutex);

  void update_dimension_bv_event(
      const std::shared_ptr<bam::pb_dimension_bv_event>& bv_event)
      ABSL_LOCKS_EXCLUDED(_mutex);
  std::shared_ptr<bam::pb_dimension_bv_event> dimension_bv(uint64_t bv_id) const
      ABSL_LOCKS_EXCLUDED(_mutex);
  void update_dimension_ba_event(
      const std::shared_ptr<bam::pb_dimension_ba_event>& ba_event)
      ABSL_LOCKS_EXCLUDED(_mutex);
  std::shared_ptr<bam::pb_dimension_ba_event> dimension_ba(uint64_t ba_id) const
      ABSL_LOCKS_EXCLUDED(_mutex);
  void update_index_mapping(
      const std::shared_ptr<storage::pb_index_mapping>& index_mapping)
      ABSL_LOCKS_EXCLUDED(_mutex);
  void remove_index_mapping(uint64_t host_id, uint64_t service_id)
      ABSL_LOCKS_EXCLUDED(_mutex);
  void update_acknowledgement(
      const std::shared_ptr<neb::pb_acknowledgement>& ack)
      ABSL_LOCKS_EXCLUDED(_mutex);
};
}  // namespace cache
}  // namespace com::centreon::broker

#endif  // !CCB_CACHE_BROKER_CACHE_HH
