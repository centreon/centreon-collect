/**
 * Copyright 2024 Centreon
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

#ifndef CCE_MOD_OTL_CENTREON_AGENT_AGENT_IMPL_HH
#define CCE_MOD_OTL_CENTREON_AGENT_AGENT_IMPL_HH

#include "agent_stat.hh"

#include "com/centreon/engine/modules/opentelemetry/centreon_agent/agent_config.hh"
#include "com/centreon/engine/modules/opentelemetry/otl_data_point.hh"

#include "com/centreon/engine/modules/opentelemetry/conf_helper.hh"

namespace com::centreon::engine::modules::opentelemetry::centreon_agent {

/**
 * @brief base class of agent connection.
 * It contains static containers that allow to find agent connection from
 * service@host ids
 *
 */
class agent_impl_base : public std::enable_shared_from_this<agent_impl_base> {
  struct instance_element {
    instance_element() {}
    instance_element(uint64_t host_id,
                     uint64_t serv_id,
                     const std::shared_ptr<agent_impl_base>& conn)
        : host_serv(host_id, serv_id), connection(conn) {}

    using host_serv_pair =
        std::pair<uint64_t /*host_id*/, uint64_t /*serv_id*/>;
    host_serv_pair host_serv;
    std::shared_ptr<agent_impl_base> connection;
  };

  using instance_container = boost::multi_index::multi_index_container<
      instance_element,
      boost::multi_index::indexed_by<
          boost::multi_index::ordered_non_unique<BOOST_MULTI_INDEX_MEMBER(
              instance_element,
              instance_element::host_serv_pair,
              host_serv)>,
          boost::multi_index::ordered_non_unique<BOOST_MULTI_INDEX_MEMBER(
              instance_element,
              std::shared_ptr<agent_impl_base>,
              connection)>>>;

  static instance_container* _configured_instance
      ABSL_GUARDED_BY(*_instances_m);
  static absl::btree_set<std::shared_ptr<agent_impl_base>>*
      _no_configured_instance ABSL_GUARDED_BY(*_instances_m);
  static absl::Mutex* _instances_m;

 protected:
  void _on_new_conf(const agent::AgentConfiguration& conf);

  void _on_done();

  virtual void _force_check(uint64_t host_id, uint64_t serv_id) = 0;

  template <typename applier>
  static void _apply_to_all(applier&& apply);

 public:
  using pointer = std::shared_ptr<agent_impl_base>;

  virtual ~agent_impl_base() = default;

  virtual void shutdown() = 0;

  static void force_check(uint64_t host_id, uint64_t serv_id);

  static void all_agent_calc_and_send_config_if_needed(
      const agent_config::pointer& new_conf);

  virtual void calc_and_send_config_if_needed(
      const agent_config::pointer& new_conf) = 0;
};

/**
 * @brief call apply(agent_impl_base::pointer) on each connection stored in
 * containers
 *
 * @tparam applier
 * @param apply
 */
template <typename applier>
void agent_impl_base::_apply_to_all(applier&& apply) {
  std::vector<pointer> all_conn;
  {
    absl::MutexLock l(*_instances_m);
    all_conn.reserve(_no_configured_instance->size() +
                     _configured_instance->size());
    std::copy(_no_configured_instance->begin(), _no_configured_instance->end(),
              std::back_inserter(all_conn));

    pointer last = nullptr;
    for (const auto& to_copy : _configured_instance->get<1>()) {
      if (to_copy.connection != last) {
        all_conn.push_back(to_copy.connection);
        last = to_copy.connection;
      }
    }
  }
  std::for_each(all_conn.begin(), all_conn.end(), apply);
}

/**
 * @brief this class manages connection with centreon monitoring agent
 * reverse connection or no
 *
 * @tparam bireactor_class (grpc::bireactor<,>)
 */
template <class bireactor_class>
class agent_impl : public bireactor_class, public agent_impl_base {
  std::shared_ptr<boost::asio::io_context> _io_context;
  const std::string_view _class_name;
  const bool _reversed;
  const bool _is_crypted;
  const std::chrono::system_clock::time_point _exp_time;

  whitelist_cache _whitelist_cache;

  agent_config::pointer _conf ABSL_GUARDED_BY(_protect);
  bool _agent_can_receive_encrypted_credentials ABSL_GUARDED_BY(_protect);

  metric_handler _metric_handler;

  std::shared_ptr<agent::MessageFromAgent> _agent_info
      ABSL_GUARDED_BY(_protect);
  std::shared_ptr<agent::MessageToAgent> _last_sent_config
      ABSL_GUARDED_BY(_protect);

  bool _write_pending;
  std::deque<std::shared_ptr<agent::MessageToAgent>> _write_queue
      ABSL_GUARDED_BY(_protect);
  std::shared_ptr<agent::MessageFromAgent> _read_current
      ABSL_GUARDED_BY(_protect);

  void _calc_and_send_config_if_needed();

  virtual const std::string& get_peer() const = 0;

  void _write(const std::shared_ptr<agent::MessageToAgent>& request);

 protected:
  std::shared_ptr<spdlog::logger> _logger;
  bool _alive ABSL_GUARDED_BY(_protect);

  agent_stat::pointer _stats;

  mutable absl::Mutex _protect;

  void _force_check(uint64_t host_id, uint64_t serv_id) override;

 public:
  agent_impl(const std::shared_ptr<boost::asio::io_context>& io_context,
             const std::string_view class_name,
             const agent_config::pointer& conf,
             const metric_handler& handler,
             const std::shared_ptr<spdlog::logger>& logger,
             bool reversed,
             bool is_crypted,
             const agent_stat::pointer& stats);

  agent_impl(const std::shared_ptr<boost::asio::io_context>& io_context,
             const std::string_view class_name,
             const agent_config::pointer& conf,
             const metric_handler& handler,
             const std::shared_ptr<spdlog::logger>& logger,
             bool reversed,
             bool is_crypted,
             const agent_stat::pointer& stats,
             const std::chrono::system_clock::time_point& exp_time);

  virtual ~agent_impl();

  std::shared_ptr<agent_impl<bireactor_class>> shared_from_this() {
    return std::static_pointer_cast<agent_impl<bireactor_class>>(
        agent_impl_base::shared_from_this());
  }

  void calc_and_send_config_if_needed(
      const agent_config::pointer& new_conf) override;

  static void update_config();

  void on_request(const std::shared_ptr<agent::MessageFromAgent>& request);

  static void register_stream(const std::shared_ptr<agent_impl>& strm);

  void start_read();

  void start_write();

  // bireactor part
  void OnReadDone(bool ok) override;

  virtual void on_error() = 0;

  void OnWriteDone(bool ok) override;

  // server version
  void OnDone();
  // client version
  void OnDone(const ::grpc::Status& /*s*/);

  void shutdown() override;

  static void shutdown_all();
};

}  // namespace com::centreon::engine::modules::opentelemetry::centreon_agent

#endif
