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
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index_container.hpp>
#include <filesystem>

#include "com/centreon/broker/bam/internal.hh"
#include "com/centreon/broker/neb/internal.hh"
#include "com/centreon/broker/neb/service_status.hh"

namespace com::centreon::engine::configuration {
class State;
class Host;
class Service;
}  // namespace com::centreon::engine::configuration

namespace com::centreon::broker {

class Host;
class Service;

struct by_id {};
struct by_name {};
struct by_service {};
struct by_instance {};
struct by_severity {};

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
  absl::flat_hash_map<uint64_t, std::string> _instances ABSL_GUARDED_BY(_mutex);

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

  /* Active (started) downtimes to persist with the cache (set by broker_state
   * just before the downtime_manager is unloaded). */
  std::vector<Downtime> _active_downtimes_to_save ABSL_GUARDED_BY(_mutex);
  /* Active downtimes read back from the cache file, awaiting re-injection into
   * the downtime_manager once their host/service is known to the cache. */
  std::vector<Downtime> _pending_active_downtimes ABSL_GUARDED_BY(_mutex);

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
  void _publish(const std::shared_ptr<io::data>& to_publish)
      ABSL_LOCKS_EXCLUDED(_mutex);
  void _load_cache() ABSL_LOCKS_EXCLUDED(_mutex);
  void _save_cache() ABSL_LOCKS_EXCLUDED(_mutex);

 public:
  broker_cache(std::shared_ptr<spdlog::logger> logger);
  broker_cache(const broker_cache&) = delete;
  broker_cache& operator=(const broker_cache&) = delete;
  ~broker_cache() noexcept;

  void enable_section(uint32_t sections) noexcept {
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
};
}  // namespace cache
}  // namespace com::centreon::broker

#endif  // !CCB_CACHE_BROKER_CACHE_HH
