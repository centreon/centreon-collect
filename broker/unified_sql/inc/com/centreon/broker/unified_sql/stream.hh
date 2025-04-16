/**
 * Copyright 2019-2024 Centreon
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

#ifndef CCB_UNIFIED_SQL_STREAM_HH
#define CCB_UNIFIED_SQL_STREAM_HH

#include "bbdo/neb.pb.h"
#include "com/centreon/broker/io/events.hh"
#include "com/centreon/broker/io/stream.hh"
#include "com/centreon/broker/misc/shared_mutex.hh"
#include "com/centreon/broker/sql/mysql_multi_insert.hh"
#include "com/centreon/broker/unified_sql/bulk_bind.hh"
#include "com/centreon/broker/unified_sql/bulk_queries.hh"
#include "com/centreon/broker/unified_sql/rebuilder.hh"
#include "com/centreon/broker/unified_sql/stored_timestamp.hh"
#include "com/centreon/common/perfdata.hh"

namespace com::centreon::broker {
namespace unified_sql {
struct int64_not_minus_one {
  int64_t value;
};

struct uint64_not_null_not_neg_1 {
  uint64_t value;
};

struct uint64_not_null {
  uint64_t value;
};

}  // namespace unified_sql
}  // namespace com::centreon::broker

namespace fmt {
template <>
struct formatter<com::centreon::broker::unified_sql::int64_not_minus_one> {
  constexpr auto parse(format_parse_context& ctx)
      -> format_parse_context::iterator {
    return ctx.begin();
  }
  auto format(const com::centreon::broker::unified_sql::int64_not_minus_one& v,
              format_context& ctx) const -> format_context::iterator {
    // ctx.out() is an output iterator to write to.
    return v.value > 0 ? fmt::format_to(ctx.out(), "{}", v.value)
                       : fmt::format_to(ctx.out(), "NULL");
  }
};

template <>
struct formatter<
    com::centreon::broker::unified_sql::uint64_not_null_not_neg_1> {
  constexpr auto parse(format_parse_context& ctx)
      -> format_parse_context::iterator {
    return ctx.begin();
  }
  auto format(
      const com::centreon::broker::unified_sql::uint64_not_null_not_neg_1& v,
      format_context& ctx) const -> format_context::iterator {
    // ctx.out() is an output iterator to write to.
    return v.value > 0 && v.value != static_cast<uint64_t>(-1)
               ? fmt::format_to(ctx.out(), "{}", v.value)
               : fmt::format_to(ctx.out(), "NULL");
  }
};

template <>
struct formatter<com::centreon::broker::unified_sql::uint64_not_null> {
  constexpr auto parse(format_parse_context& ctx)
      -> format_parse_context::iterator {
    return ctx.begin();
  }
  auto format(const com::centreon::broker::unified_sql::uint64_not_null& v,
              format_context& ctx) const -> format_context::iterator {
    // ctx.out() is an output iterator to write to.
    return v.value > 0 ? fmt::format_to(ctx.out(), "{}", v.value)
                       : fmt::format_to(ctx.out(), "NULL");
  }
};

}  // namespace fmt

namespace com::centreon::broker {
/* Forward declarations */
namespace neb {
class service_status;
}

namespace unified_sql {

constexpr const char* BAM_NAME = "_Module_";
constexpr int32_t dt_queue_timer_duration = 5;

/**
 * @brief The conflict manager.
 *
 * Many queries are executed by Broker through the sql connector and also
 * the unified_sql connector. Thos queries are made with several connections to
 * the database and we don't commit after each query. All those constraints
 * are there for performance purpose but they can lead us to database
 * deadlocks. To avoid such locks, there is the conflict manager. Sent queries
 * are sent to connections through it. The idea behind the conflict manager
 * is the following:
 *
 * * determine the connection to use for the upcoming query.
 * * Check that the "action" to execute is compatible with actions already
 *   running on our connection and also others. If not, solve the issue with
 *   commits.
 * * Send the query to the connection.
 * * Add or not an action flag to this connection for next queries.
 *
 * Another task of the conflict manager is to keep informations for queries.
 * Metrics, customvariables are sent in bulk to avoid locks on the database,
 * so we keep some containers here to build those big queries.
 *
 * The conflict manager works with two streams: sql and unified_sql.
 *
 * To initialize it, two functions are used:
 * * init_unified_sql(): initialization of the unified_sql part. This one needs
 * the sql part to be initialized before. If it is not already initialized, this
 *   function waits for it (with a timeout).
 *
 * Once the object is initialized, we have the classical static internal method
 * `instance()`.
 */
class stream : public io::stream {
  /* Forward declarations */
 public:
  enum instance_state { not_started, running, finished };
  enum stream_type { sql, unified_sql };

 private:
  const static std::array<int, 5> hst_ordered_status;
  const static std::array<int, 5> svc_ordered_status;
  enum special_conn {
    custom_variable,
    downtime,
    host_group,
    host_parent,
    log,
    service_group,
    severity,
    tag,
    comment
  };

  enum actions {
    none = 0,
    acknowledgements = 1 << 0,
    comments = 1 << 1,
    custom_variables = 1 << 2,
    downtimes = 1 << 3,
    host_hostgroups = 1 << 4,
    host_parents = 1 << 5,
    hostgroups = 1 << 6,
    hosts = 1 << 7,
    instances = 1 << 8,
    modules = 1 << 9,
    service_servicegroups = 1 << 10,
    servicegroups = 1 << 11,
    services = 1 << 12,
    index_data = 1 << 13,
    metrics = 1 << 14,
    severities = 1 << 15,
    tags = 1 << 16,
    resources = 1 << 17,
    resources_tags = 1 << 18,
  };

  struct index_info {
    uint64_t index_id;
    std::string host_name;
    std::string service_description;
    uint32_t rrd_retention;
    uint32_t interval;
    bool special;
    bool locked;
  };

  static const std::array<std::string, 5> metric_type_name;

  struct metric_info {
    bool locked;
    uint32_t metric_id;
    uint32_t type;
    float value;
    std::string unit_name;
    float warn;
    float warn_low;
    bool warn_mode;
    float crit;
    float crit_low;
    bool crit_mode;
    float min;
    float max;
    bool metric_mapping_sent;
  };

  instance_state _state;

  mutable std::mutex _fifo_m;
  std::atomic_int _processed;
  std::atomic_int _ack;

  std::atomic_int _pending_events;
  uint32_t _count;
  bool _bulk_prepared_statement = false;

  /* Current actions by connection */
  std::vector<uint32_t> _action;

  // bool _exit;
  uint32_t _loop_timeout;
  uint32_t _max_pending_queries;
  database_config _dbcfg;
  mysql _mysql;
  std::unique_ptr<mysql> _dedicated_connections;
  uint32_t _instance_timeout;
  rebuilder _rebuilder;
  bool _store_in_db = true;
  bool _store_in_resources;
  bool _store_in_hosts_services;
  uint32_t _rrd_len = 0u;
  uint32_t _interval_length = 0u;
  uint32_t _max_perfdata_queries = 0u;
  uint32_t _max_metrics_queries = 0u;
  uint32_t _max_cv_queries = 0u;
  uint32_t _max_log_queries = 0u;
  uint32_t _max_dt_queries = 0u;

  std::time_t _next_update_metrics;
  std::time_t _next_loop_timeout;

  asio::steady_timer _queues_timer ABSL_GUARDED_BY(_timer_m);
  /* To give the order to stop the check_queues */
  std::atomic_bool _stop_check_queues;
  /* When the check_queues is really stopped */
  bool _check_queues_stopped;

  /* Stats */
  std::shared_ptr<stats::center> _center;
  ConflictManagerStats* _stats;

  absl::flat_hash_set<uint32_t> _cache_deleted_instance_id;
  std::unordered_map<uint32_t, uint32_t> _cache_host_instance;
  absl::flat_hash_map<uint64_t, size_t> _cache_hst_cmd;
  absl::flat_hash_map<std::pair<uint64_t, uint64_t>, size_t> _cache_svc_cmd;
  absl::flat_hash_map<std::pair<uint64_t, uint64_t>, index_info> _index_cache;

  absl::flat_hash_map<std::pair<uint64_t, std::string>, metric_info>
      _metric_cache;
  misc::shared_mutex _metric_cache_m;

  absl::flat_hash_map<std::pair<uint64_t, uint16_t>, uint64_t> _severity_cache;
  absl::flat_hash_map<std::pair<uint64_t, uint16_t>, uint64_t> _tags_cache;

  absl::flat_hash_map<std::pair<uint64_t, uint64_t>, uint64_t> _resource_cache;

  mutable absl::Mutex _timer_m;
  /* This is a barrier for timers. It must be locked in shared mode in the
   * timers functions. So we can execute several timer functions at the same
   * time. But it is locked in write mode in the stream destructor. So When
   * executed, we are sure that all the timer functions have finished. */
  mutable absl::Mutex _barrier_timer_m;
  asio::system_timer _group_clean_timer ABSL_GUARDED_BY(_timer_m);
  asio::system_timer _loop_timer ABSL_GUARDED_BY(_timer_m);

  /* loggers  */
  std::shared_ptr<spdlog::logger> _logger_sql;
  std::shared_ptr<spdlog::logger> _logger_sto;

  absl::flat_hash_set<uint32_t> _hostgroup_cache;
  absl::flat_hash_set<uint32_t> _servicegroup_cache;

  /* The queue of metrics sent in bulk to the database. The insert is done if
   * the loop timeout is reached or if the queue size is greater than
   * _max_perfdata_queries. The filled table here is 'data_bin'. */
  mutable std::mutex _queues_m;
  mutable std::condition_variable _queues_cond_var;
  /* This map is also sent in bulk to the database. The insert is done if
   * the loop timeout is reached or if the queue size is greater than
   * _max_metrics_queries. Values here are the real time values, so if the
   * same metric is recevied two times, the new value can overwrite the old
   * one, that's why we store those values in a map. The filled table here is
   * 'metrics'. */
  std::unordered_map<int32_t, metric_info> _metrics;

  /* These queues are sent in bulk to the database. The insert/update is done
   * if the loop timeout is reached or if the queue size is greater than
   * _max_cv_queries/_max_log_queries. */
  bulk_queries _cv;
  bulk_queries _cvs;

  std::unique_ptr<database::bulk_or_multi> _perfdata_query;

  std::unique_ptr<database::bulk_or_multi> _logs;
  std::unique_ptr<database::bulk_or_multi> _downtimes;
  std::unique_ptr<database::bulk_or_multi> _comments;

  timestamp _oldest_timestamp;
  std::mutex _stored_timestamps_m;
  std::unordered_map<uint32_t, stored_timestamp> _stored_timestamps;

  database::mysql_stmt _acknowledgement_insupdate;
  database::mysql_stmt _pb_acknowledgement_insupdate;
  database::mysql_stmt _custom_variable_delete;
  database::mysql_stmt _flapping_status_insupdate;
  database::mysql_stmt _host_check_update;
  database::mysql_stmt _pb_host_check_update;
  database::mysql_stmt _host_group_insupdate;
  database::mysql_stmt _pb_host_group_insupdate;
  database::mysql_stmt _host_group_member_delete;
  database::mysql_stmt _host_group_member_insert;
  database::mysql_stmt _pb_host_group_member_insert;
  database::mysql_stmt _host_insupdate;
  database::mysql_stmt _pb_host_insupdate;
  database::mysql_stmt _host_parent_delete;
  database::mysql_stmt _host_parent_insert;
  database::mysql_stmt _pb_host_parent_delete;
  database::mysql_stmt _pb_host_parent_insert;
  database::mysql_stmt _host_status_update;
  database::mysql_stmt _instance_insupdate;
  database::mysql_stmt _pb_instance_insupdate;
  database::mysql_stmt _instance_status_insupdate;
  database::mysql_stmt _pb_instance_status_insupdate;
  database::mysql_stmt _service_check_update;
  database::mysql_stmt _pb_service_check_update;
  database::mysql_stmt _service_group_insupdate;
  database::mysql_stmt _pb_service_group_insupdate;
  database::mysql_stmt _service_group_member_delete;
  database::mysql_stmt _service_group_member_insert;
  database::mysql_stmt _pb_service_group_member_delete;
  database::mysql_stmt _pb_service_group_member_insert;
  database::mysql_stmt _service_insupdate;
  database::mysql_stmt _pb_service_insupdate;
  database::mysql_stmt _service_status_update;

  std::unique_ptr<database::mysql_stmt_base> _hscr_update;
  std::unique_ptr<bulk_bind> _hscr_bind;

  std::unique_ptr<database::mysql_stmt_base> _sscr_update;
  std::unique_ptr<bulk_bind> _sscr_bind;

  /* Statement and binding to enable hosts in the hosts table. One value is
   * set at index 0 that is the host ID. */
  std::unique_ptr<database::mysql_stmt_base> _eh_update;
  std::unique_ptr<bulk_bind> _eh_bind;

  /* Statement and binding to enable hosts in the resources table. One value
   * is set at index 0 that is the host ID. */
  std::unique_ptr<database::mysql_stmt_base> _ehr_update;
  std::unique_ptr<bulk_bind> _ehr_bind;

  /* Statement and binding to enable services in the services table. One value
   * is set at index 0 that is the service ID. */
  std::unique_ptr<database::mysql_stmt_base> _es_update;
  std::unique_ptr<bulk_bind> _es_bind;

  /* Statement and binding to enable services in the resources table. One value
   * is set at index 0 that is the service ID. */
  std::unique_ptr<database::mysql_stmt_base> _esr_update;
  std::unique_ptr<bulk_bind> _esr_bind;

  database::mysql_stmt _severity_insert;
  database::mysql_stmt _severity_update;
  database::mysql_stmt _tag_insert_update;
  database::mysql_stmt _tag_delete;
  database::mysql_stmt _resources_tags_insert;
  database::mysql_stmt _resources_host_insert;
  database::mysql_stmt _resources_host_update;
  database::mysql_stmt _resources_service_insert;
  database::mysql_stmt _resources_service_update;

  database::mysql_stmt _resources_disable;
  database::mysql_stmt _resources_tags_remove;

  std::unique_ptr<database::mysql_stmt_base> _hscr_resources_update;
  std::unique_ptr<bulk_bind> _hscr_resources_bind;

  std::unique_ptr<database::mysql_stmt_base> _sscr_resources_update;
  std::unique_ptr<bulk_bind> _sscr_resources_bind;

  static const std::string _index_data_insert_request;
  database::mysql_stmt _index_data_insert;
  database::mysql_stmt _index_data_update;
  database::mysql_stmt _index_data_query;
  database::mysql_stmt _metrics_insert;

  database::mysql_stmt _agent_information_insert_update;

  void _update_hosts_and_services_of_unresponsive_instances();
  void _update_hosts_and_services_of_instance(uint32_t id, bool responsive);
  void _update_timestamp(uint32_t instance_id);
  bool _is_valid_poller(uint32_t instance_id);
  void _check_queues(boost::system::error_code ec)
      ABSL_SHARED_LOCKS_REQUIRED(_barrier_timer_m);
  void _check_deleted_index();
  void _check_rebuild_index();

  void _process_acknowledgement(const std::shared_ptr<io::data>& d);
  void _process_pb_acknowledgement(const std::shared_ptr<io::data>& d);
  void _process_comment(const std::shared_ptr<io::data>& d);
  void _process_pb_comment(const std::shared_ptr<io::data>& d);
  void _process_custom_variable(const std::shared_ptr<io::data>& d);
  void _process_custom_variable_status(const std::shared_ptr<io::data>& d);
  void _process_pb_custom_variable(const std::shared_ptr<io::data>& d);
  void _process_pb_custom_variable_status(const std::shared_ptr<io::data>& d);
  void _process_downtime(const std::shared_ptr<io::data>& d);
  void _process_pb_downtime(const std::shared_ptr<io::data>& d);
  void _process_host_check(const std::shared_ptr<io::data>& d);
  void _process_pb_host_check(const std::shared_ptr<io::data>& d);
  void _process_host_group(const std::shared_ptr<io::data>& d);
  void _process_pb_host_group(const std::shared_ptr<io::data>& d);
  void _process_host_group_member(const std::shared_ptr<io::data>& d);
  void _process_pb_host_group_member(const std::shared_ptr<io::data>& d);
  void _process_host(const std::shared_ptr<io::data>& d);
  void _process_host_parent(const std::shared_ptr<io::data>& d);
  void _process_pb_host_parent(const std::shared_ptr<io::data>& d);
  void _process_host_status(const std::shared_ptr<io::data>& d);
  void _process_instance(const std::shared_ptr<io::data>& d);
  void _process_pb_instance(const std::shared_ptr<io::data>& d);
  void _process_instance_status(const std::shared_ptr<io::data>& d);
  void _process_pb_instance_status(const std::shared_ptr<io::data>& d);
  void _process_log(const std::shared_ptr<io::data>& d);
  void _process_service_check(const std::shared_ptr<io::data>& d);
  void _process_pb_service_check(const std::shared_ptr<io::data>& d);
  void _process_service_group(const std::shared_ptr<io::data>& d);
  void _process_pb_service_group(const std::shared_ptr<io::data>& d);
  void _process_service_group_member(const std::shared_ptr<io::data>& d);
  void _process_pb_service_group_member(const std::shared_ptr<io::data>& d);
  void _process_service(const std::shared_ptr<io::data>& d);
  void _process_service_status(const std::shared_ptr<io::data>& d);
  void _process_responsive_instance(const std::shared_ptr<io::data>& d);

  void _process_pb_host(const std::shared_ptr<io::data>& d);
  uint64_t _process_pb_host_in_resources(const Host& h, int32_t conn);
  void _process_pb_instance_configuration(const std::shared_ptr<io::data>& d);
  void _process_pb_host_status(const std::shared_ptr<io::data>& d);
  void _process_pb_adaptive_host_status(const std::shared_ptr<io::data>& d);
  void _process_pb_adaptive_host(const std::shared_ptr<io::data>& d);
  void _process_pb_service(const std::shared_ptr<io::data>& d);
  uint64_t _process_pb_service_in_resources(const Service& s, int32_t conn);
  void _process_pb_adaptive_service(const std::shared_ptr<io::data>& d);
  void _process_pb_service_status(const std::shared_ptr<io::data>& d);
  void _process_pb_adaptive_service_status(const std::shared_ptr<io::data>& d);
  void _process_severity(const std::shared_ptr<io::data>& d);
  void _process_tag(const std::shared_ptr<io::data>& d);
  void _process_pb_log(const std::shared_ptr<io::data>& d);
  void _process_pb_responsive_instance(const std::shared_ptr<io::data>& d);
  void _process_agent_stats(const std::shared_ptr<io::data>& d);
  void _unified_sql_process_service_status(const std::shared_ptr<io::data>& d);
  void _check_and_update_index_cache(const Service& ss);

  void _unified_sql_process_pb_service_status(
      const std::shared_ptr<io::data>& d);

  void _load_deleted_instances();
  void _init_statements();
  void _load_caches();
  void _clean_tables(uint32_t instance_id);
  void _clean_group_table() ABSL_SHARED_LOCKS_REQUIRED(_barrier_timer_m);
  void _prepare_hg_insupdate_statement();
  void _prepare_pb_hg_insupdate_statement();
  void _prepare_sg_insupdate_statement();
  void _prepare_pb_sg_insupdate_statement();
  void _finish_action(int32_t conn, uint32_t action);
  void _finish_actions();
  void _add_action(int32_t conn, actions action);
  void _update_metrics();
  // void __exit();
  void _clear_instances_cache(const std::list<uint64_t>& ids);
  bool _host_instance_known(uint64_t host_id) const;

  void _start_loop_timer() ABSL_EXCLUSIVE_LOCKS_REQUIRED(_timer_m);

 public:
  static void (stream::*const neb_processing_table[])(
      const std::shared_ptr<io::data>&);

  stream(database_config const& dbcfg,
         uint32_t rrd_len,
         uint32_t interval_length,
         uint32_t loop_timeout,
         uint32_t instance_timeout,
         bool store_in_data_bin,
         bool store_in_resources,
         bool store_in_hosts_services);
  stream() = delete;
  stream& operator=(const stream&) = delete;
  stream(const stream&) = delete;
  ~stream() noexcept ABSL_LOCKS_EXCLUDED(_barrier_timer_m);

  static const multiplexing::muxer_filter& get_muxer_filter();
  static const multiplexing::muxer_filter& get_forbidden_filter();

  void update_metric_info_cache(uint64_t index_id,
                                uint32_t metric_id,
                                std::string const& metric_name,
                                short metric_type);
  int32_t write(const std::shared_ptr<io::data>& d) override;
  int32_t flush() override;
  bool read(std::shared_ptr<io::data>& d, time_t deadline = -1) override;
  int32_t stop() override;
  void statistics(nlohmann::json& tree) const override;
  void remove_graphs(const std::shared_ptr<io::data>& d);
  void remove_poller(const std::shared_ptr<io::data>& d);
  void process_stop(const std::shared_ptr<io::data>& d);
  void update() override;
};
}  // namespace unified_sql
}  // namespace com::centreon::broker

#endif /* !CCB_UNIFIED_SQL_STREAM_HH */
